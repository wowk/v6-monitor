#include "event.h"
#include "link.h"
#include "debug.h"
#include "rtnlmsg.h"

enum {
    LAN_MODE_RA,
    LAN_MODE_PD,
    LAN_MODE_PT,
};

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

static int start_ra_mode()
{
    debug("%s: start dhcp6s and start radvd", __func__);
	system("rc dhcp6s stop");
	system("rc radvd start");
    system("ndproxy -w eth0.1 -l br0 -rd -a 150 -t 1");

    return 0;
}

static int start_pd_mode()
{
    debug("%s: start dhcp6s and start radvd", __func__);
	system("rc dhcp6s start");
    system("rc radvd stop");

    return 0;
}

static int stop_v6_paththrough()
{
    debug("%s", __FUNCTION__);
    system("brctl delif br0 eth0.ipv6");
    system("ifconfig eth0.ipv6 down");
    system("vlanctl --if eth0 --rx --tags 0 --set-rxif eth0.ipv6 --filter-ethertype 0x86dd --rule-remove-by-filter");
    system("vlanctl --if eth0 --tx --tags 0 --filter-ethertype 0x86dd --rule-remove-by-filter");
    system("vlanctl --if-delete eth0.ipv6");

    return 0;
}

static int stop_ra_mode()
{
    debug("%s: stop radvd", __func__);
	system("rc radvd stop");
    system("rc dhcp6s stop");
    system("killall -SIGTERM ndproxy");

    return 0;
}

static int stop_pd_mode()
{
    debug("%s: stop radvd & dhcp6s", __func__);
	system("rc radvd stop");
	system("rc dhcp6s stop");

    return 0;
}

int handle_event(struct v6_monitor* v6_monitor)
{
    if( event_happened(v6_monitor, EVENT_SWITCH_TO_PD) ){
        debug("Got Switch To PD event");
        if( v6_monitor->lan_mode != LAN_MODE_PD ){
            stop_v6_paththrough();
            stop_ra_mode();
            start_pd_mode();
            v6_monitor->lan_mode = LAN_MODE_PD;
            debug("switch to PD mode");
        }else{
            debug("already in PD mode, ignore this event");
        }
    }else if(event_happened(v6_monitor, EVENT_SWITCH_TO_PT)){
        debug("Got Switch To PT event");
        if( v6_monitor->lan_mode != LAN_MODE_PT ){
            stop_ra_mode();
            stop_pd_mode();
            start_v6_paththrough();
            v6_monitor->lan_mode = LAN_MODE_PT;
            debug("switch to PT mode");
        }else{
            debug("already in PT mode, ignore this event");
        }
    }else if(event_happened(v6_monitor, EVENT_SWITCH_TO_RA)){
        debug("Got Switch To RA event");
        if( v6_monitor->lan_mode != LAN_MODE_RA ){
            stop_v6_paththrough();
            stop_pd_mode();
            start_ra_mode();
            v6_monitor->lan_mode = LAN_MODE_RA;
            debug("switch to RA mode");
        }else{
            debug("already in RA mode, ignore this event");
        }
    }

    if( event_happened(v6_monitor, EVENT_DHCP6_RECONFIG) ){
        debug("Got RECONFIG event");
        if( v6_monitor->lan_mode == LAN_MODE_PD ){
            system("killall -USR1 radvd");
            system("killall -USR1 dhcp6s");
            debug("reconfig lan host, send RA packet to lan host");
        }else{
            debug("Not in PD mode, ignore this event");
        }
    }

    if( event_happened(v6_monitor, EVENT_ND_PROXY_DISABLE) ){
        debug("Got ND_PROXY_DISABLE event");
        if( v6_monitor->lan_mode == LAN_MODE_RA ){
            stop_ra_mode();
            start_v6_paththrough();
            debug("start ipv6 path through");
        }else{
            debug("Not in RA mode, ignore this event");
        }
    }else if( event_happened(v6_monitor, EVENT_ND_PROXY_ENABLE) ){
        debug("Got ND_PROXY_ENABLE event");
        if( v6_monitor->lan_mode ){
            stop_v6_paththrough();
            start_ra_mode();
            debug("stop ipv6 path through");
        }else{
            debug("Not in RA mode, ignore this event");
        }
    }

    if( event_happened(v6_monitor, EVENT_RADVD_DISABLE) ){
        debug("Got RADVD DISABLE event");
        if( v6_monitor->lan_mode == LAN_MODE_RA ){
            system("rc radvd stop");
            debug("stop RADVD");
        }else{
            debug("Not in RA mode, ignore this event");
        }
    }else if(event_happened(v6_monitor, EVENT_RADVD_ENABLE)){
        debug("Got RADVD ENABLE event");
        if( v6_monitor->lan_mode == LAN_MODE_RA ){
            system("rc radvd restart");
            debug("start RADVD");
        }else{
            debug("Not in RA mode, ignore this event");
        }
    }

    if( event_happened(v6_monitor, EVENT_DHCP6_MODIFIED) ){
        debug("Got DHCP6 MODIFIED Event");
        if( v6_monitor->lan_mode == LAN_MODE_RA ){
            system("killall -USR1 radvd");
            debug("stop RADVD");
        }else{
            debug("Not in RA mode, ignore this event");
        }
    }

    return 0;
}

void set_event(struct v6_monitor *v6_monitor, v6_monitor_event_code_t ev_code, void *arg)
{
    if( v6_monitor->event[ev_code].event_arg ){
        free(v6_monitor->event[ev_code].event_arg);
    }
    v6_monitor->event[ev_code].event_arg = arg;
    v6_monitor->event[ev_code].triggerred = true;
}

bool event_happened(struct v6_monitor *v6_monitor, v6_monitor_event_code_t ev_code)
{
    return v6_monitor->event[ev_code].triggerred;
}

void clear_event(struct v6_monitor *v6_monitor, v6_monitor_event_code_t ev_code)
{

    if( v6_monitor->event[ev_code].event_arg ){
        free(v6_monitor->event[ev_code].event_arg);
        v6_monitor->event[ev_code].event_arg = NULL;
    }
    v6_monitor->event[ev_code].triggerred = false;
}

void clear_all_event(struct v6_monitor *v6_monitor)
{
    for( int i = EVENT_MIN ; i < EVENT_MAX ; i ++ ){
        clear_event(v6_monitor, v6_monitor->event[i].code);
    }
}
