#ifndef LINK_H__
#define LINK_H__

#include "salinfo.h"
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>


TAILQ_HEAD(v6addrs_t, v6addr_t);

struct v6addr_t {
    socklen_t prefix_len;
    struct in6_addr addr;
    TAILQ_ENTRY(v6addr_t) entry;
};

struct link_t {
    uint16_t link_flags_mask;
    uint16_t link_flags;
    uint16_t link_index;
    uint32_t wan_id;
    struct v6addrs_t v6prefixs;
    struct v6addrs_t v6addrs;
    uint32_t link_event;
    struct sal_info_t sal_info;
};

#endif
