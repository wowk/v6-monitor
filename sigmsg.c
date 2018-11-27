#include "sigmsg.h"
#include "debug.h"
#include "event.h"
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <ipv6_event.h>
#include <sys/signalfd.h>


int handle_signal_event(struct v6_monitor* v6_monitor)
{
    ssize_t retlen = 0;
    struct signalfd_siginfo sfd_siginfo;

    retlen = read(v6_monitor->signalfd, &sfd_siginfo, sizeof(sfd_siginfo));
    if ( 0 > retlen  ) {
        error(0, errno, "failed to read signal message");
        if( errno != EINTR ){
            return -errno;
        }else{
            return 0;
        }
    }
	
    if ( sfd_siginfo.ssi_signo == SIGUSR2 ) {
		return 0;
    }
	
    if( sfd_siginfo.ssi_signo == SIGTERM ){
        info("get SIGTERM");
        v6_monitor->running = false;
        return 0;
    }

    if( sfd_siginfo.ssi_signo == SIGINT ){
        info("get SIGINT");
        v6_monitor->running = false;
        return 0;
    }

	if( sfd_siginfo.ssi_code == SI_QUEUE ){
		info("SIGNAL not send by calling sigqueue, ignore it");
        return 0;
	}

	if ( sfd_siginfo.ssi_signo == SIGUSR1 ) {
    	if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_DHCP6_RECONFIG ){
            set_event(v6_monitor, EVENT_DHCP6_RECONFIG, 0);
		}
		else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_RADVD_ENABLE){
            set_event(v6_monitor, EVENT_RADVD_ENABLE, 0);
		}
		else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_RADVD_DISABLE){
            set_event(v6_monitor, EVENT_RADVD_DISABLE, 0);
		}
		else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_ND_PROXY_ENABLE){
            set_event(v6_monitor, EVENT_ND_PROXY_ENABLE, 0);
		}
		else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_ND_PROXY_DISABLE){
            set_event(v6_monitor, EVENT_ND_PROXY_DISABLE, 0);
		}
        else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_PD ){
            set_event(v6_monitor, EVENT_SWITCH_TO_PD, 0);
            clear_event(v6_monitor, EVENT_SWITCH_TO_PD);
            clear_event(v6_monitor, EVENT_SWITCH_TO_RA);
        }
        else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_RA ){
            set_event(v6_monitor, EVENT_SWITCH_TO_RA, 0);
            clear_event(v6_monitor, EVENT_SWITCH_TO_PD);
            clear_event(v6_monitor, EVENT_SWITCH_TO_PT);
        }
        else if( sfd_siginfo.ssi_int == V6_MONITOR_EVENT_PT ){
            set_event(v6_monitor, EVENT_SWITCH_TO_PT, 0);
            clear_event(v6_monitor, EVENT_SWITCH_TO_PD);
            clear_event(v6_monitor, EVENT_SWITCH_TO_RA);
        }else{
            info("got event code: %d, ignore it", sfd_siginfo.ssi_int);
        }
    }

    return 0;
}


