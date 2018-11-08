#ifndef RTNLMSG_H__
#define RTNLMSG_H__

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <linux/rtnetlink.h>

#define IF_PREFIX_ONLINK	0x01
#define IF_PREFIX_AUTOCONF	0x02

TAILQ_HEAD(v6addrs_t, v6addr_t);

enum link_event_t {
    EVENT_NEW_ADDR,
    EVENT_MORE_ADDR,
    EVENT_ADD_PREFIX,
    EVENT_NEW_PREFIX,
    EVENT_DEL_ADDR,
    EVENT_LOST_ADDR,
    EVENT_DEL_LINK,
    EVENT_LINK_RUNNING,
    EVENT_LINK_STOP,
    EVENT_DHCP6_MODIFIED,
};

struct v6addr_t {
    socklen_t prefix_len;
    struct in6_addr addr;
    TAILQ_ENTRY(v6addr_t) entry;
};

struct dhcp_info_t {
    char* dnsaddr;
    char* prefix;
    char* ipaddr;
    char* netmask;
    char* phone_number;
    char* ntp_server;
    char* sntp_server;
    char* sip_domain;
    
};

struct link_t {
    uint16_t link_flags_mask;
    uint16_t link_flags;
    uint16_t link_index;
    uint32_t link_wan_id;
    struct v6addrs_t v6prefixs;
    struct v6addrs_t v6addrs;
    uint32_t link_event;
    struct dhcp_info_t dhcp_info; 
};

struct rtattrs_t {
    struct rtattr* tb[128];
    uint64_t mask;
};

extern int create_rtnl_mc_socket(uint32_t nl_groups);
extern int handle_rtnl_mc_msg(const char* msg, size_t msglen, struct link_t* link);
extern void delete_all_address_from_link(struct link_t* link);

#endif
