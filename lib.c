#define _GNU_SOURCE

#include "debug.h"
#include "lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>


int parse_args(int argc, char* argv[], struct option_args_t* args)
{
    int op;

    while ( 0 <= (op = getopt(argc, argv, "w:i:t:")) ) {
        if ( op == 'w' ) {
            args->wan_id = atoi(optarg);
        } else if ( op == 'i' ) {
            args->ifindex = if_nametoindex(optarg);
        } else if ( op == 't' )  {
            args->timeout = atoi(optarg);
        } else if ( op == '?' ) {
            return -1;
        }
    }
    args->appname = argv[0];

    return 0;
}

int create_pid_file(const char* appname)
{
    FILE* fp = NULL;
    pid_t pid;
    char pid_file[FILENAME_MAX] = "";

    snprintf(pid_file, sizeof(pid_file),"/var/run/%s.pid", appname);
    if ( access(pid_file, R_OK)  < 0 ) {
        info("create pid file");
        fp = fopen(pid_file, "w");
    } else {
        info("found pid file");
        fp = fopen(pid_file, "rw");
        fscanf(fp, "%u", &pid);
        char process[32] = "";
        snprintf(process, sizeof(process), "/proc/%u/cmdline", (unsigned)pid);
        if ( access(process, R_OK) == 0 ) {
            error(0, EEXIST, "a ipv6_mr process is running");
            return -EEXIST;
        } else {
            fclose(fp);
            fp = fopen(pid_file, "w");
            info("process logged in pid file is not exist: %s", process);
        }
    }
    if ( !fp ) {
        error(0, errno, "cant create pid file for ipv6_mgr");
        return -errno;
    }

    fprintf(fp, "%u", (unsigned)getpid());
    fclose(fp);

    return 0;
}

int create_timerfd(uint32_t interval)
{
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    if( fd < 0 ){
        error(0, errno, "failed to create timerfd_create");
        return -errno;
    }

    struct itimerspec its = {
        .it_interval.tv_sec     = interval,
        .it_interval.tv_nsec    = 0,
        .it_value.tv_sec        = interval,
        .it_value.tv_nsec       = 0,
    };

    if( 0 > timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, NULL)){
        error(0, errno, "failed to timerfd_settime");
        close(fd);
        return -errno;
    }

    return fd;
}

int create_signalfd(unsigned sig_cnt, ...)
{
    sigset_t sigset;
    sigemptyset(&sigset);

    va_list val;
    va_start(val, sig_cnt);
    for( unsigned i = 0 ; i < sig_cnt ; i ++ ){
        sigaddset(&sigset, va_arg(val, int));
    }
    sigprocmask(SIG_BLOCK, &sigset, NULL);

    int fd = signalfd(-1, &sigset, SFD_CLOEXEC);
    if( fd < 0 ){
        error(0, errno, "failed to create signalfd");
        return -errno;
    }

    return fd;
}
