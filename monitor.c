#include "debug.h"
#include "lib.h"
#include "monitor.h"
#include "rtnlmsg.h"
#include <sys/inotify.h>


struct v6_monitor* create_v6_monitor(int argc, char *argv[])
{
    struct v6_monitor *monitor;

    monitor = (struct v6_monitor*)calloc(1, sizeof(struct v6_monitor));
    if( !monitor ){
        error(0, errno, "failed to create v6_monoitor");
        return NULL;
    }

    monitor->args.appname   = argv[0];
    monitor->args.timeout   = 1;
    monitor->args.wan_id    = 0;
    monitor->args.timer_interval = 1;

    if ( 0 > parse_args(argc, argv, &monitor->args) ) {
        error(0, 0, "usage: %s < -i interface > [ -w wan_id] [ -t timeout ]\n", argv[0]);
        monitor->err_code = errno;
    }

    if ( 0 > create_pid_file(monitor->args.appname) ) {
        error(0, errno, "failed to create pid file for v6_monoitor");
        monitor->err_code = errno;
    }

    monitor->timerfd = create_timerfd(monitor->args.timer_interval);
    if( 0 > monitor->timerfd ){
        error(0, errno, "failed to create timerfd for v6_monoitor");
        monitor->err_code = errno;
    }

    init_sal_info(&monitor->link.sal_info);
    monitor->watchfd = inotify_init1(IN_CLOEXEC);
    if ( 0 > monitor->watchfd ) {
        error(0, errno, "failed to init inotify");
        monitor->err_code = errno;
    }

    char sal_file[32] = "";
    sprintf(sal_file, "/tmp/0/%u/s000", monitor->args.wan_id);
    if ( 0 > inotify_add_watch(monitor->watchfd, sal_file, IN_MODIFY) ) {
        error(0, errno, "failed to watch file %s", sal_file);
        monitor->err_code = errno;
    }

    unsigned nl_groups = 0;
    nl_groups |= RTMGRP_IPV6_IFADDR;
    nl_groups |= RTMGRP_IPV6_PREFIX;
    nl_groups |= RTMGRP_LINK;
    monitor->netlinkfd = create_rtnl_mc_socket(nl_groups);
    if ( 0 > monitor->netlinkfd ) {
        error(0, errno, "failed to create rtnetlink socket for v6_monoitor");
        monitor->err_code = errno;
    }

    monitor->signalfd = create_signalfd(4, SIGUSR1, SIGUSR2, SIGTERM, SIGINT);
    if( 0 > monitor->signalfd ){
        error(0, errno, "failed to create signalfd for v6_monoitor");
        monitor->err_code = errno;
    }

    return monitor;
}

void cleanup_v6_monitor(struct v6_monitor* monitor)
{
    if( monitor->netlinkfd >= 0 ){
        close(monitor->netlinkfd);
    }
    if( monitor->signalfd >= 0 ){
        close(monitor->signalfd);
    }
    if( monitor->timerfd >= 0 ){
        close(monitor->timerfd);
    }
    if( monitor->watchfd >= 0 ){
        close(monitor->watchfd);
    }

    clear_all_event(monitor);

    free(monitor);
}
