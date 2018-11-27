#include "monitor.h"
#include "debug.h"
#include "event.h"
#include "lib.h"
#include "rtnlmsg.h"
#include "salinfo.h"
#include "sigmsg.h"
#include "addrs.h"
#include <time.h>


int main(int argc, char **argv)
{
    int ret = 0;
    struct v6_monitor* v6_monitor;

    v6_monitor = create_v6_monitor(argc, argv);
    if( !v6_monitor ){
        return -errno;
    }else if( v6_monitor->err_code ){
        cleanup_v6_monitor(v6_monitor);
        return v6_monitor->err_code;
    }

    struct timeval tv = {
        .tv_sec	= v6_monitor->args.timeout,
        .tv_usec	= 0,
    };

    int maxfd;
    maxfd = max(v6_monitor->netlinkfd, v6_monitor->timerfd);
    maxfd = max(v6_monitor->signalfd, maxfd);
    maxfd = max(v6_monitor->watchfd, maxfd);

    fd_set rfdset, rfdset_save;
    FD_ZERO(&rfdset_save);
    FD_SET(v6_monitor->netlinkfd, &rfdset_save);
    FD_SET(v6_monitor->signalfd, &rfdset_save);
    FD_SET(v6_monitor->timerfd, &rfdset_save);
    FD_SET(v6_monitor->watchfd, &rfdset_save);

    v6_monitor->running = true;
    while(v6_monitor->running) {
        struct timeval timeout = tv;
        rfdset = rfdset_save;

        ret = select(maxfd + 1, &rfdset, NULL, NULL, &timeout);
        if (0 > ret ) {
            if( errno != EINTR ){
                error(0, errno, "failed to select socket");
                v6_monitor->running = false;
            }
            continue;
        }
        else if (ret == 0) {
            handle_event(v6_monitor);
            continue;
        }

        if (FD_ISSET(v6_monitor->netlinkfd, &rfdset)) {
            ret = handle_netlink_event(v6_monitor);
        }
        
        if (FD_ISSET(v6_monitor->signalfd, &rfdset)) {
            ret = handle_signal_event(v6_monitor);
        }

        if (FD_ISSET(v6_monitor->watchfd, &rfdset)) {
            ret = handle_sal_event(v6_monitor);
        }

        if ( ret < 0 ) {
            v6_monitor->running = false;
        }
    }

    cleanup_v6_monitor(v6_monitor);

    return ret;
}


