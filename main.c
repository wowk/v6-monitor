#include "debug.h"
#include "rtnlmsg.h"
#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>


static volatile bool running = true;


struct option_args_t {
    int wan_id;
    int ifindex;
    int timeout;
};

static int parse_args(int argc, char** argv, struct option_args_t* args)
{
    int op;

    while ( 0 <= (op = getopt(argc, argv, "w:i:t:")) ) {
        if( op == 'w' ){
            args->wan_id = atoi(optarg);
        }else if ( op == 'i' ) {
            args->ifindex = if_nametoindex(optarg);
        } else if ( op == 't' )  {
            args->timeout = atoi(optarg);
        } else if ( op == '?' ) {
            return -1;
        }
    }

    return 0;
}

static int create_pid_file(void)
{
	FILE* fp = NULL;
    pid_t pid;
	//const char* pid_file = "/var/run/ipv6_mgr.pid";
	const char* pid_file = "ipv6_mgr.pid";

    if ( access(pid_file, R_OK)  < 0 ) {
        info("create pid file");
        fp = fopen(pid_file, "w");
    } else {
        info("found pid file");
        fp = fopen(pid_file, "rw");
        fscanf(fp, "%u", &pid);
        char process[32] = "";
        snprintf(process, sizeof(process), "/proc/%u/cmdline", (unsigned)pid);
        if ( access(process, R_OK) == 0 ) {
            error(EEXIST, "a ipv6_mr process is running");
            return -EEXIST;
        } else {
            fclose(fp);
            fp = fopen(pid_file, "w");
            info("process logged in pid file is not exist: %s", process);
        }
    }
    if ( !fp ) {
        error(errno, "cant create pid file for ipv6_mgr");
        return -errno;
    }

    fprintf(fp, "%u", (unsigned)getpid());
    fclose(fp);

	return 0;
}

int add_wlan_multicast_rule(struct link_t* link) 
{
    const char *iflist[] = {"wl0", "wl1", NULL};
    
    system("ip6tables -F FWD_MULTICAST");
    system("ip6tables -F FORWARD");
    system("ip6tables -X FWD_MULTICAST");
    system("ip6tables -N FWD_MULTICAST");
    
    for( int i = 0 ; iflist[i] != NULL ; i ++){
        char cmd[256] = "";
        snprintf(cmd, sizeof(cmd), "ip6tables -D FORWARD -o %s -d ff00::/8 -j FWD_MULTICAST", iflist[i]);
        snprintf(cmd, sizeof(cmd), "ip6tables -A FORWARD -o %s -d ff00::/8 -j FWD_MULTICAST", iflist[i]);
        system(cmd);
    }

    system("ip6tables -A FWD_MULTICAST -p icmpv6 --icmpv6-type 133 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p icmpv6 --icmpv6-type 134 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p icmpv6 --icmpv6-type 135 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p icmpv6 --icmpv6-type 136 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p icmpv6 --icmpv6-type 137 -j ACCEPT");

    system("ip6tables -A FWD_MULTICAST -p tcp -d ff02::fb --dport 5353 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p udp -d ff02::fb --dport 5353 -j ACCEPT");
    
    system("ip6tables -A FWD_MULTICAST -p udp --dport 546:547 -j ACCEPT");
    system("ip6tables -A FWD_MULTICAST -p tcp --dport 546:547 -j ACCEPT");
    
    system("ip6tables -A FWD_MULTICAST -j DROP");
    
    return 0;
}

static int get_if_flags(uint32_t ifindex, uint16_t* flags)
{
	int fd;
    char ifname[IF_NAMESIZE] = "";
    struct ifreq ifr;
    
    if( !if_indextoname(ifindex, ifname) ){
        return -errno;
    }
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, ifname, sizeof(ifname));
    
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( fd < 0 ) {
        error(errno, "failed to create socket to get interface's flags");
        return -errno;
    } else if( 0 > ioctl(fd, SIOCGIFFLAGS, &ifr) ){
        error(errno, "failed to ioctl socket to get interface's flags");
        close(fd);
        return -errno;
    }
    close(fd);
    *flags = ifr.ifr_flags;

    debug("link_flags: 0x%.4x", *flags);

	return 0;
}

static int handle_netlink_event(int fd, struct link_t* link)
{
	int ret = 0;
	ssize_t retlen = 0;
	char buffer[8192] = "";

	while ( 0 > (retlen = recv(fd, buffer, sizeof(buffer), 0)) && errno == EINTR );
	if ( 0 > retlen ) {
		ret = -errno;
		error(errno, "failed to recv netlink msg from kernel");
    }
	else if (0 >  handle_rtnl_mc_msg(buffer, retlen, link)) {
		ret = -errno;
		error(errno, "failed to handle event msg");
	}

	return ret;
}

static int check_and_reload_dhcp_info(struct link_t* link, bool* changed)
{
    return 0;
}

static int handle_dhcp_event(int fd, struct link_t* link)
{
	int ret = 0;
	bool modified = false;
	size_t len = 0;
	ssize_t retlen = 0;
	char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    info("func: %s", __FUNCTION__);
	while( 0 > (retlen = read(fd, buffer, sizeof(buffer))) && errno == EINTR);
	if( retlen < 0 ){
        error(errno, "failed to read inode event message");
		ret = -errno;
		return ret;
	}

	event = (struct inotify_event*)buffer;
    len = sizeof(struct inotify_event) + event->len;
	while( retlen >= len ){
		if( event->mask & IN_MODIFY ){
			modified = true;
			break;
		}
		retlen -= (sizeof(struct inotify_event) + event->len);
		event ++;
	}
    
    bool changed = false;
	if( modified && check_and_reload_dhcp_info(link, &changed) == 0 ){
        //tell dhcp server to send reconfigure message
        link->link_event |= (1 << EVENT_DHCP6_MODIFIED);
    }

	return 0;
}

static int handle_signal_event(int fd, struct link_t* link)
{
    ssize_t retlen = 0;
	struct signalfd_siginfo sfd_siginfo;

    while(0 > (retlen = read(fd, &sfd_siginfo, sizeof(sfd_siginfo))) && errno == EINTR);
    if( retlen < 0  ){
        error(errno, "failed to read signal message");
    }

    if( sfd_siginfo.ssi_signo == SIGUSR1 ){
        struct v6addr_t* addr;
        TAILQ_FOREACH(addr, &link->v6addrs, entry){
            char ipaddr[128] = "";
            inet_ntop(AF_INET6, &addr->addr, ipaddr, sizeof(ipaddr));
            info("dump address: %s", ipaddr);
        }
    }else if( sfd_siginfo.ssi_signo == SIGUSR2 ){
        info("got SIGUSR2");
    }

    return 0;
}

static int init_addrs(struct link_t* link)
{
    char ifname[IF_NAMESIZE] = "";

    if( !if_indextoname(link->link_index, ifname) ){
        error(errno, "no interface with id(%u)", link->link_index);
        return -errno;
    }

    struct ifaddrs* ifp;
    if( 0 > getifaddrs(&ifp) ){
        error(errno, "failed to get %s's addresses list", ifname);
        return -errno;
    }

    struct ifaddrs* p = ifp;
    while( p ){
        if( p->ifa_addr == NULL || p->ifa_netmask == NULL ){
            p = p->ifa_next;
            continue;
        }
        if( p->ifa_addr->sa_family != AF_INET6 ){
            p = p->ifa_next;
            continue;
        }
        if( strcmp(ifname, p->ifa_name) ){
            p = p->ifa_next;
            continue;
        }

        uint8_t* ptr; 
        uint8_t prefix_len = 0;
        struct in6_addr* addr = &((struct sockaddr_in6*)(p->ifa_addr))->sin6_addr;
        ptr = (uint8_t*)(addr->s6_addr);
        if( (ptr[0] & (uint8_t)0x70) != 0x20 ){
            p = p->ifa_next;
            continue;
        }

        ptr = ((struct sockaddr_in6*)p->ifa_netmask)->sin6_addr.s6_addr;
        for( int i = 0 ; i < 16 ; i ++ ){
            if( ptr[i] == 0xff ) {
                prefix_len += 8;
            }
            else{
                while( ptr[i] ){
                    prefix_len ++;
                    ptr[i] <<= 1;
                }
                break;
            }
        }

        struct v6addr_t* new_addr = (struct v6addr_t*)malloc(sizeof(struct v6addr_t));
        if( !new_addr ){
            freeifaddrs(ifp);
            return -ENOMEM;
        }
        new_addr->prefix_len    = prefix_len;
        memcpy(&new_addr->addr, addr, sizeof(struct in6_addr));
        TAILQ_INSERT_HEAD(&link->v6addrs, new_addr, entry);
        
        char ipaddr[128] = "";
        inet_ntop(AF_INET6, addr, ipaddr, sizeof(ipaddr));
        info("add ipaddr: %s, prefix length: %u", ipaddr, prefix_len);

        p = p->ifa_next;
    }

    freeifaddrs(ifp);

    return 0;
}

void callback_on_exit(int status_code, void* args)
{
	printf("called on exit");
}

int main(int argc, char **argv)
{
    int ret = 0;
    int evt_fd = -1;
	int sig_fd = -1;
	int inode_fd = -1;
	int maxfd;
	char sal_file[FILENAME_MAX] = "";
	uint32_t nl_groups;
#if 1
	sigset_t sigset;
#endif
	struct timeval tv;
    struct timeval timeout;
    struct  option_args_t args;
    struct link_t link;


	ret = create_pid_file();
	if( ret < 0 ){
        error(errno, "failed to create pid file");
		goto RETURN_ERROR;
	}

    args.timeout = 5;
    args.ifindex = -1;
	args.wan_id = 0;
    if ( 0 > (ret = parse_args(argc, argv, &args)) || args.ifindex < 0 ) {
        printf("usage: %s < -i interface > [ -w wan_id] [ -t timeout ]\n", argv[0]);
        goto RETURN_ERROR;
    }

    memset(&link, 0, sizeof(link));
	ret = get_if_flags(args.ifindex, &link.link_flags);
	if( ret < 0 ){
        error(errno, "failed to get inferface's flag");
		goto RETURN_ERROR;
	}
    link.link_wan_id = args.wan_id;
    link.link_index  = args.ifindex;
	TAILQ_INIT(&link.v6addrs);
	TAILQ_INIT(&link.v6prefixs);

    if( 0 > init_addrs(&link) ){
        error(errno, "failed to init addrs list");
        goto RETURN_ERROR;
    }

    if( !(link.link_flags & IFF_RUNNING) && !TAILQ_EMPTY(&link.v6addrs) ){

        delete_all_address_from_link(&link);
    }

#if 1
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
    sigprocmask(SIG_SETMASK, &sigset, NULL);
	sig_fd = signalfd(-1, &sigset, SFD_CLOEXEC);
	if( sig_fd < 0 ){
		ret = -errno;
        error(errno, "failed to init signal fd");
		goto RETURN_ERROR;
	}
#endif

	inode_fd = inotify_init1(IN_CLOEXEC);
	if( inode_fd < 0 ){
		ret = -errno;
        error(errno, "failed to init inotify");
		goto RETURN_ERROR;
	}
	sprintf(sal_file, "/tmp/0/%u/s000", args.wan_id);
	if( 0 > inotify_add_watch(inode_fd, sal_file, IN_MODIFY) ){
        error(errno, "failed to watch file %s", sal_file);
        goto RETURN_ERROR;
    }

    nl_groups = 0;
    nl_groups |= RTMGRP_IPV6_IFADDR;
    nl_groups |= RTMGRP_IPV6_PREFIX;
    nl_groups |= RTMGRP_LINK;

    evt_fd = create_rtnl_mc_socket(nl_groups);
    if ( evt_fd < 0 ) {
		ret = -errno;
        error(evt_fd, "fail to create rtnl mc socket");
        goto RETURN_ERROR;
    }

	on_exit(callback_on_exit, &link);
    add_wlan_multicast_rule(&link);

	tv.tv_sec	= args.timeout;
	tv.tv_usec	= 0;
	if( evt_fd > inode_fd && evt_fd > sig_fd ){
		maxfd = evt_fd;
	}else{
		maxfd = inode_fd > sig_fd ? inode_fd : sig_fd;
	}

    fd_set rfdset, rfdset_save;
    FD_ZERO(&rfdset_save);
    FD_SET(evt_fd, &rfdset_save);
	//FD_SET(sig_fd, &rfdset_save);
	FD_SET(inode_fd, &rfdset_save);

    while (running) {
        tv = timeout;
        rfdset = rfdset_save;

        while( 0 > (ret = select(maxfd + 1, &rfdset, NULL, NULL, &tv)) && errno == EINTR);
        if (ret < 0) {
            error(errno, "failed to select socket");
            goto RETURN_ERROR;
        }
        
		else if (ret == 0) {
            handle_link_event(&link);
            continue;
        }

		if (FD_ISSET(evt_fd, &rfdset)) {
			ret = handle_netlink_event(evt_fd, &link);
        }
#if 1
		if (FD_ISSET(sig_fd, &rfdset)){
			ret = handle_signal_event(sig_fd, &link);
		}
#endif
		if (FD_ISSET(inode_fd, &rfdset)){
			ret = handle_dhcp_event(inode_fd, &link);
		}

		if( ret < 0 ){
			goto RETURN_ERROR;
		}
    }

    ret = 0;

RETURN_ERROR:

    struct v6addr_t* addr = NULL;
    struct v6addr_t* deleted = NULL;
    TAILQ_FOREACH(addr, &link.v6addrs, entry){
        if( deleted ){
            free(deleted);
            deleted = NULL;
        }
        TAILQ_REMOVE(&link.v6addrs, addr, entry);
    }
    if( deleted ){
        free(deleted);
    }

    deleted = NULL;
    TAILQ_FOREACH(addr, &link.v6prefixs, entry){
        if( deleted ){
            free(deleted);
            deleted = NULL;
        }
        TAILQ_REMOVE(&link.v6addrs, addr, entry);
    }
    if( deleted ){
        free(deleted);
    }

    if (evt_fd >= 0) {
        close(evt_fd);
    }

    if (sig_fd >= 0){
        close(sig_fd);
    }

    if(inode_fd >= 0){
        close(inode_fd);
    }

    return ret;
}
