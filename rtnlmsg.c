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



static int handle_new_addr_event(struct ifaddrmsg* ifa, struct rtattrs_t* rta, struct link_t* link)
{
    if(ifa->ifa_index != link->link_index)
        return 0;
    
    if( !rta->tb[IFA_ADDRESS] || RT_SCOPE_UNIVERSE != ifa->ifa_scope ){
        return 0;
    }
     
    char ipaddr[128] = "";
    inet_ntop(AF_INET6, RTA_DATA(rta->tb[IFA_ADDRESS]), ipaddr, sizeof(ipaddr));
    struct v6addr_t* addr;
    TAILQ_FOREACH(addr, &link->v6addrs, entry) {
        if( !memcmp(&addr->addr, RTA_DATA(rta->tb[IFA_ADDRESS]), sizeof(struct in6_addr)) ){
            debug("address %s exist, ignore it\n", ipaddr);
            return 0;
        }
    }

    struct v6addr_t* new_addr = (struct v6addr_t*)malloc(sizeof(struct v6addr_t));
    if( !new_addr ){
        return -ENOMEM;
    }

    if( TAILQ_EMPTY(&link->v6addrs) ){
        link->link_event |= (1 << EVENT_NEW_ADDR);
        info("new addr: %s\n", ipaddr);
    }else{
        link->link_event |= (1 << EVENT_MORE_ADDR);
        info("more addr: %s\n", ipaddr);
    }
    new_addr->prefix_len = ifa->ifa_prefixlen;
    memcpy(&new_addr->addr, RTA_DATA(rta->tb[IFA_ADDRESS]), sizeof(struct in6_addr));
    TAILQ_INSERT_HEAD(&link->v6addrs, new_addr, entry);

    return 0;
}

static int handle_new_link_event(struct ifinfomsg* ifi, struct rtattrs_t* rta, struct link_t* link)
{
    info("%s", __FUNCTION__);
    if(ifi->ifi_index != link->link_index)
        return 0;
    
    link->link_flags = ifi->ifi_flags;

    if( link->link_flags & IFF_RUNNING ){
        link->link_event |= (1 << EVENT_LINK_RUNNING);
    }else{
        link->link_event |= (1 << EVENT_LINK_STOP);
        delete_all_address_from_link(link);
    }

    return 0;
}

static int handle_prefix_event(struct prefixmsg* pref, struct rtattrs_t* rta, struct link_t* link)
{
    info("%s", __FUNCTION__);
    //struct prefix_cacheinfo* pref_cache;

    if(pref->prefix_ifindex != link->link_index)
        return 0;
    
    if( !rta->tb[PREFIX_ADDRESS] ){
        return 0;
    }

    if(!(pref->prefix_flags & IF_PREFIX_AUTOCONF))
        return 0;
    
    uint8_t* paddr = (uint8_t*)(((struct in6_addr*)RTA_DATA(rta->tb[PREFIX_ADDRESS]))->s6_addr);
    if( (paddr[0] & 0x07) != 0x02 ){
        return 0;
    }

    char ipaddr[128] = "";
    inet_ntop(AF_INET6, RTA_DATA(rta->tb[PREFIX_ADDRESS]), ipaddr, sizeof(ipaddr));
    
    struct v6addr_t* addr = NULL;
    TAILQ_FOREACH(addr, &link->v6prefixs, entry){
        if( addr->prefix_len != pref->prefix_len ){
            continue;
        }

        if( !memcmp(&addr->addr, RTA_DATA(rta->tb[PREFIX_ADDRESS]), sizeof(struct in6_addr)) ){
            debug("prefix %s is exist, ignore", ipaddr);
            return 0;
        }
    }
    struct v6addr_t* new_addr = (struct v6addr_t*)malloc(sizeof(struct v6addr_t));
    if( !new_addr ){
        return -ENOMEM;
    }
    new_addr->prefix_len = pref->prefix_len;
    memcpy(&new_addr->addr, RTA_DATA(rta->tb[PREFIX_ADDRESS]), sizeof(struct in6_addr));
    TAILQ_INSERT_HEAD(&link->v6prefixs, new_addr, entry);
    info("prefix: %s", ipaddr);
    link->link_event |= (1 << EVENT_ADD_PREFIX);

    return 0;
}

static int handle_del_addr_event(struct ifaddrmsg* ifa, struct rtattrs_t* rta, struct link_t* link)
{
    info("%s", __FUNCTION__);
    if( ifa->ifa_index != link->link_index )
        return 0;
    
    if( !rta->tb[IFA_ADDRESS] || ifa->ifa_scope != RT_SCOPE_UNIVERSE ){
        return 0;
    }
    
    char ipaddr[128] = "";
    inet_ntop(AF_INET6, RTA_DATA(rta->tb[IFA_ADDRESS]), ipaddr, sizeof(ipaddr));
    struct v6addr_t* addr;
    TAILQ_FOREACH(addr, &link->v6addrs, entry) {
        if( !memcmp(&addr->addr, RTA_DATA(rta->tb[IFA_ADDRESS]), sizeof(struct in6_addr)) ){
            break;
        }
    }

    if( !addr ){
        return 0;
    }

    info("remove addr: %s", ipaddr);
    TAILQ_REMOVE(&link->v6addrs, addr, entry);
    free(addr);
    
    if( TAILQ_EMPTY(&link->v6addrs) ){
        link->link_event &= ~(1u << EVENT_DEL_ADDR);
        link->link_event |= (1u << EVENT_LOST_ADDR);
    }else{
        link->link_event |= (1u << EVENT_DEL_ADDR);
    }

    return 0;
}

static int handle_del_link_event(struct ifinfomsg* ifi, struct rtattrs_t* rta, struct link_t* link)
{
    if(ifi->ifi_index != link->link_index)
        return 0;

    delete_all_address_from_link(link);

    link->link_event |= (1 << EVENT_LINK_STOP);
    
    return 0;
}

static int parse_rtattr(struct rtattr* rtattr, short max, size_t payload_len, struct rtattrs_t* rtas)
{
    rtas->mask = 0;

    memset(rtas, 0, sizeof(struct rtattrs_t));

    while( payload_len && RTA_OK(rtattr, payload_len) ) {
        if( max >= rtattr->rta_type && !rtas->tb[rtattr->rta_type]) {
            rtas->tb[rtattr->rta_type] = rtattr;
        }
        rtattr = RTA_NEXT(rtattr, payload_len);
    }

    return 0;
}

int handle_rtnl_mc_msg(const char* msg, size_t msglen, struct link_t* link)
{
    int ret;
    struct rtattr* rtattr;
    struct rtattrs_t rtattrs;
    struct nlmsghdr *hdr = (struct nlmsghdr *)msg;

    while( msglen && NLMSG_OK(hdr, msglen) ) {

        if( hdr->nlmsg_type == RTM_NEWPREFIX ) {
            rtattr = RTM_RTA((struct prefixmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, RTM_MAX, RTM_PAYLOAD(hdr), &rtattrs);
            ret = handle_prefix_event((struct prefixmsg*)NLMSG_DATA(hdr), &rtattrs, link);
        }

        else if( hdr->nlmsg_type == RTM_NEWLINK ) {
            rtattr = IFLA_RTA((struct ifinfomsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFLA_MAX, IFLA_PAYLOAD(hdr), &rtattrs);
            ret = handle_new_link_event((struct ifinfomsg*)NLMSG_DATA(hdr), &rtattrs, link);
        }

        else if( hdr->nlmsg_type == RTM_DELLINK ) {
            rtattr = IFLA_RTA((struct ifinfomsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFLA_MAX, IFLA_PAYLOAD(hdr), &rtattrs);
            ret = handle_del_link_event((struct ifinfomsg*)NLMSG_DATA(hdr), &rtattrs, link);
        }

        else if( hdr->nlmsg_type == RTM_NEWADDR ) {
            rtattr = IFA_RTA((struct ifaddrmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFA_MAX, IFA_PAYLOAD(hdr), &rtattrs);
            ret = handle_new_addr_event((struct ifaddrmsg*)NLMSG_DATA(hdr), &rtattrs, link);
        }

        else if( hdr->nlmsg_type == RTM_DELADDR ) {
            rtattr = IFA_RTA((struct ifaddrmsg*)NLMSG_DATA(hdr));
            parse_rtattr(rtattr, IFA_MAX, IFA_PAYLOAD(hdr), &rtattrs);
            ret = handle_del_addr_event((struct ifaddrmsg*)NLMSG_DATA(hdr), &rtattrs, link);
        }

        else if( hdr->nlmsg_type == NLMSG_DONE ) {
            break;
        }

        if( ret < 0 ) {
            return ret;
        }

        hdr = NLMSG_NEXT(hdr, msglen);
    }

    return 0;
}

int create_rtnl_mc_socket(uint32_t nl_groups)
{
    int evt_fd;

    /* create event socket */
    evt_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (evt_fd < 0) {
        error(errno, "failed to create evt socket");
        return -errno;
    }

    struct sockaddr_nl sn = {
        .nl_family = AF_NETLINK,
        .nl_groups = nl_groups,
        .nl_pid = 0,
        .nl_pad = 0,
    };

    if (bind(evt_fd, (struct sockaddr *)&sn, sizeof(sn)) < 0) {
        error(errno, "failed to bind evt socket");
        close(evt_fd);
        return -errno;
    }

    return evt_fd;
}

void delete_all_address_from_link(struct link_t* link)
{
    char ifname[IF_NAMESIZE] = "";
    char ipaddr[128] = "";
    char cmd[256] = "";
    struct v6addr_t* addr;

    if( !if_indextoname(link->link_index, ifname) ){
        error(0, "no such interface");
        return;
    }
    
    TAILQ_FOREACH(addr, &link->v6addrs, entry) {
        inet_ntop(AF_INET6, &addr->addr, ipaddr, sizeof(ipaddr));
        sprintf(cmd, "ip address delete %s/%u dev %s", ipaddr, addr->prefix_len, ifname);
        debug(cmd);
        system(cmd);    
    }
}
