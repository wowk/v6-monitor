#ifndef RTNLMSG_H__
#define RTNLMSG_H__

#include "link.h"
#include <stdlib.h>
#include <stdbool.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>
#include "monitor.h"


#define IF_PREFIX_ONLINK	0x01
#define IF_PREFIX_AUTOCONF	0x02

struct rtattrs_t {
    struct rtattr* tb[128];
    uint64_t mask;
};

extern int create_rtnl_mc_socket(uint32_t nl_groups);
extern void delete_all_address_from_link(struct link_t* link);

extern int handle_netlink_event(struct v6_monitor* v6_monitor);

#endif
