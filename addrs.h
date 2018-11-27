#ifndef ADDRS_H__
#define ADDRS_H__

#include "link.h"
#include <stdint.h>


extern int get_if_flags(uint32_t ifindex, uint16_t* flags);
extern int init_addrs(struct link_t* link);

#endif
