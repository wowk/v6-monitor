#include "debug.h"
#include "addrs.h"
#include <ifaddrs.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>


int get_if_flags(uint32_t ifindex, uint16_t* flags)
{
    int fd;
    char ifname[IF_NAMESIZE] = "";
    struct ifreq ifr;

    if ( !if_indextoname(ifindex, ifname) ) {
        return -errno;
    }
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, ifname, sizeof(ifname));

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( fd < 0 ) {
        error(0, errno, "failed to create socket to get interface's flags");
        return -errno;
    } else if ( 0 > ioctl(fd, SIOCGIFFLAGS, &ifr) ) {
        error(0, errno, "failed to ioctl socket to get interface's flags");
        close(fd);
        return -errno;
    }
    close(fd);
    *flags = ifr.ifr_flags;

    debug("link_flags: 0x%.4x", *flags);

    return 0;
}

int init_addrs(struct link_t* link)
{
    char ifname[IF_NAMESIZE] = "";

    if ( !if_indextoname(link->link_index, ifname) ) {
        error(errno, "no interface with id(%u)", link->link_index);
        return -errno;
    }

    struct ifaddrs* ifp;
    if ( 0 > getifaddrs(&ifp) ) {
        error(errno, "failed to get %s's addresses list", ifname);
        return -errno;
    }

    struct ifaddrs* p = ifp;
    while ( p ) {
        if ( p->ifa_addr == NULL || p->ifa_netmask == NULL ) {
            p = p->ifa_next;
            continue;
        }
        if ( p->ifa_addr->sa_family != AF_INET6 ) {
            p = p->ifa_next;
            continue;
        }
        if ( strcmp(ifname, p->ifa_name) ) {
            p = p->ifa_next;
            continue;
        }

        uint8_t* ptr;
        uint8_t prefix_len = 0;
        struct in6_addr* addr = &((struct sockaddr_in6*)(p->ifa_addr))->sin6_addr;
        ptr = (uint8_t*)(addr->s6_addr);
        if ( (ptr[0] & (uint8_t)0x70) != 0x20 ) {
            p = p->ifa_next;
            continue;
        }

        ptr = ((struct sockaddr_in6*)p->ifa_netmask)->sin6_addr.s6_addr;
        for ( int i = 0 ; i < 16 ; i ++ ) {
            if ( ptr[i] == 0xff ) {
                prefix_len += 8;
            }
            else {
                while ( ptr[i] ) {
                    prefix_len ++;
                    ptr[i] <<= 1;
                }
                break;
            }
        }

        struct v6addr_t* new_addr = (struct v6addr_t*)malloc(sizeof(struct v6addr_t));
        if ( !new_addr ) {
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


