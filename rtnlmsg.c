#include "rtnlmsg.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>


static int handle_new_addr_event(struct v6_monitor* v6_monitor, struct ifaddrmsg* ifa, struct rtattrs_t* rta)
{
    return 0;
}

static int handle_new_link_event(struct v6_monitor* v6_monitor, struct ifinfomsg* ifi, struct rtattrs_t* rta)
{
    return 0;
}

static int handle_prefix_event(struct v6_monitor* v6_monitor, struct prefixmsg* pref, struct rtattrs_t* rta)
{
    return 0;
}

static int handle_del_addr_event(struct v6_monitor* v6_monitor, struct ifaddrmsg* ifa, struct rtattrs_t* rta)
{
    return 0;
}

static int handle_del_link_event(struct v6_monitor* v6_monitor, struct ifinfomsg* ifi, struct rtattrs_t* rta)
{
    return 0;
}

static int parse_rtattr(struct rtattr* rtattr, short max, size_t payload_len, struct rtattrs_t* rtas)
{
    rtas->mask = 0;

    memset(rtas, 0, sizeof(struct rtattrs_t));

    while ( payload_len && RTA_OK(rtattr, payload_len) ) {
        if ( max >= rtattr->rta_type && !rtas->tb[rtattr->rta_type]) {
            rtas->tb[rtattr->rta_type] = rtattr;
        }
        rtattr = RTA_NEXT(rtattr, payload_len);
    }

    return 0;
}

int handle_rtnl_mc_msg(struct v6_monitor* v6_monitor, const char* msg, size_t msglen)
{
    int ret;
    struct rtattr* rtattr;
    struct rtattrs_t rtattrs;
    struct nlmsghdr *hdr = (struct nlmsghdr *)msg;

    while ( msglen && NLMSG_OK(hdr, msglen) ) {

        if ( hdr->nlmsg_type == RTM_NEWPREFIX ) {
            rtattr = RTM_RTA((struct prefixmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, RTM_MAX, RTM_PAYLOAD(hdr), &rtattrs);
            ret = handle_prefix_event(v6_monitor, (struct prefixmsg*)NLMSG_DATA(hdr), &rtattrs);
        }

        else if ( hdr->nlmsg_type == RTM_NEWLINK ) {
            rtattr = IFLA_RTA((struct ifinfomsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFLA_MAX, IFLA_PAYLOAD(hdr), &rtattrs);
            ret = handle_new_link_event(v6_monitor, (struct ifinfomsg*)NLMSG_DATA(hdr), &rtattrs);
        }

        else if ( hdr->nlmsg_type == RTM_DELLINK ) {
            rtattr = IFLA_RTA((struct ifinfomsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFLA_MAX, IFLA_PAYLOAD(hdr), &rtattrs);
            ret = handle_del_link_event(v6_monitor, (struct ifinfomsg*)NLMSG_DATA(hdr), &rtattrs);
        }

        else if ( hdr->nlmsg_type == RTM_NEWADDR ) {
            rtattr = IFA_RTA((struct ifaddrmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFA_MAX, IFA_PAYLOAD(hdr), &rtattrs);
            ret = handle_new_addr_event(v6_monitor, (struct ifaddrmsg*)NLMSG_DATA(hdr), &rtattrs);
        }

        else if ( hdr->nlmsg_type == RTM_DELADDR ) {
            rtattr = IFA_RTA((struct ifaddrmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFA_MAX, IFA_PAYLOAD(hdr), &rtattrs);
            ret = handle_del_addr_event(v6_monitor, (struct ifaddrmsg*)NLMSG_DATA(hdr), &rtattrs);
        }

        else if ( hdr->nlmsg_type == NLMSG_DONE ) {
            break;
        }

        if ( ret < 0 ) {
            return ret;
        }

        hdr = NLMSG_NEXT(hdr, msglen);
    }

    return 0;
}

int create_rtnl_mc_socket(unsigned nl_groups)
{
    int fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if( fd < 0 ){
        error(0, errno, "failed to create rtnetlink mc socket");
        close(fd);
        return -errno;
    }

    struct sockaddr_nl snl = {
        .nl_family  = PF_NETLINK,
        .nl_groups  = nl_groups,
        .nl_pid     = 0,
        .nl_pad     = 0,
    };

    if( 0 > bind(fd, (struct sockaddr*)&snl, sizeof(snl)) ){
        error(0, errno, "failed to bind rtnetlink mc socket");
        close(fd);
        return -errno;
    }

    return fd;
}

void delete_all_address_from_link(struct link_t* link)
{
    char ifname[IF_NAMESIZE] = "";
    char ipaddr[128] = "";
    char cmd[256] = "";
    struct v6addr_t* addr;

    if ( !if_indextoname(link->link_index, ifname) ) {
        error(0, errno, "no such interface");
        return;
    }

    TAILQ_FOREACH(addr, &link->v6addrs, entry) {
        inet_ntop(AF_INET6, &addr->addr, ipaddr, sizeof(ipaddr));
        sprintf(cmd, "ip address delete %s/%u dev %s", ipaddr, addr->prefix_len, ifname);
        debug(cmd);
        system(cmd);
    }
}

int handle_netlink_event(struct v6_monitor* v6_monitor)
{
    int ret = 0;
    ssize_t retlen = 0;
    char buffer[8192] = "";

    retlen = recv(v6_monitor->netlinkfd, buffer, sizeof(buffer), 0);
    if ( 0 > retlen ) {
        if( retlen != EINTR ){
            error(0, errno, "failed to recv netlink msg from kernel");
            v6_monitor->running = false;
            return -errno;
        }else{
            return 0;
        }
    }
    else if (0 >  handle_rtnl_mc_msg(v6_monitor, buffer, retlen)) {
        ret = -errno;
        error(0, errno, "failed to handle event msg");
    }

    return ret;
}

