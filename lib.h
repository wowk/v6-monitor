#ifndef UTILS_H__
#define UTILS_H__

#include <stdio.h>
#include <signal.h>
#include <stdint.h>


struct option_args_t {
    int wan_id;
    int ifindex;
    int timeout;
    int timer_interval;
    const char* appname;
};

#define max(a,b) ((a) > (b) ? (a) : (b))

extern int parse_args(int argc, char* argv[], struct option_args_t* args);
extern int create_pid_file(const char* appname);
extern int delete_pid_file(const char* appname);
extern int create_timerfd(uint32_t interval);
extern int create_signalfd(unsigned sig_cnt, ...);

#endif
