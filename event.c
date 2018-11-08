#include "event.h"
#include "debug.h"
#include "rtnlmsg.h"


static int start_v6_paththrough()
{
    debug("%s", __FUNCTION__);
    
    system("vlanctl --if-create-name eth0 eth0.ipv6");
    system("vlanctl --if eth0 --rx --tags 0 --set-rxif eth0.ipv6 --filter-ethertype 0x86dd --rule-insert-before 0");
    system("vlanctl --if eth0 --tx --tags 0 --filter-ethertype 0x86dd --rule-insert-before 0");
    system("ifconfig eth0.ipv6 up");
    system("brctl addif br0 eth0.ipv6");

    return 0;
}

static int start_radvd()
{
    debug("%s", __FUNCTION__);
    return 0;//system("rc radvd start");
}

static int start_dhcpv6()
{
    debug("%s", __FUNCTION__);
    return 0;//system("rc dhcp6s stop");
}

static int stop_v6_paththrough()
{
    debug("%s", __FUNCTION__);
    system("brctl delif br0 eth0.ipv6");
    system("ifconfig eth0.ipv6 down");
    system("vlanctl --if eth0 --rx --tags 0 --set-rxif eth0.ipv6 --filter-ethertype 0x86dd --rule-remove-by-filter");
    system("vlanctl --if eth0 --tx --tags 0 --filter-ethertype 0x86dd --rule-remove-by-filter");
    system("vlanctl --if-delete eth0 eth0.ipv6");
    
    return 0;
}

static int stop_radvd()
{
    debug("%s", __FUNCTION__);
    return 0;//system("rc radvd stop");
}

enum ipv6_lan_mode_t {
    IPV6_LAN_NONE,
    IPV6_LAN_PT,
    IPV6_LAN_RA,
    IPV6_LAN_PD,
};

static enum ipv6_lan_mode_t g_v6_lan_mode = IPV6_LAN_NONE;

static enum ipv6_lan_mode_t ipv6_lan_mode()
{
    return g_v6_lan_mode;
}

static void set_ipv6_lan_mode(enum ipv6_lan_mode_t mode)
{
    g_v6_lan_mode = mode;
}

static void update_ipv6_lan_mode()
{

}

static int stop_dhcpv6()
{
    debug("%s", __FUNCTION__);
    return system("rc dhcp6s stop");
}

int handle_link_event(struct link_t* link)
{
    if( link->link_event == 0 ) {
        return 0;
    }
    uint32_t event = link->link_event;
    link->link_event = 0;

    /* process link event */
    if( event & (1 << EVENT_LINK_STOP ) ) {
        struct v6addr_t* addr, *deleted = NULL;
        TAILQ_FOREACH(addr, &link->v6addrs, entry){
            if( deleted ){
                free(deleted);
            }
            deleted = addr;
        }
        if( deleted ){
            free(deleted);
        }
        
        deleted = NULL;
        TAILQ_FOREACH(addr, &link->v6prefixs, entry){
            if( deleted ){
                free(deleted);
            }
            deleted = addr;
        }

        stop_v6_paththrough();
        stop_radvd();
        stop_dhcpv6();
       
        set_ipv6_lan_mode(IPV6_LAN_NONE);

        return 0;
    }else if( event & (1 << EVENT_LINK_RUNNING) ){
        if( !TAILQ_EMPTY(&link->v6addrs) ){
            set_ipv6_lan_mode(IPV6_LAN_PT);
            start_v6_paththrough();
            return 0;
        }
    }

    if( event & (1 << EVENT_LOST_ADDR) ){
        stop_radvd();
        stop_dhcpv6();
        start_v6_paththrough();
        set_ipv6_lan_mode(IPV6_LAN_PT);
    }else if( event & (1 << EVENT_NEW_ADDR) ){
        stop_v6_paththrough();
        update_ipv6_lan_mode();
        if( ipv6_lan_mode() == IPV6_LAN_RA ){
            info("goto RA mode");
        }else if( ipv6_lan_mode() == IPV6_LAN_PD ){
            info("goto PD mode");
        }
    }

    if( event & (1 << EVENT_DHCP6_MODIFIED) ){
        info("ipv6 info changed");
        //info("tell dhcp6s to send reconfigure");
        //system("killall -USR1 dhcp6s");
        //info("tell RADVD to send RA packet with preferred == 0 and valid == 0");
        //system("killall -USR1 radvd");
    }
    
    return 0;
}
