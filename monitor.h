#ifndef V6_MONITOR_H__
#define V6_MONITOR_H__

#include "lib.h"
#include "link.h"
#include "event.h"
#include "salinfo.h"
#include <errno.h>
#include <stdbool.h>

struct v6_monitor {
    int timerfd;
    int netlinkfd;
    int signalfd;
    int watchfd;
    int err_code;
    bool running;
    int lan_mode;
    struct link_t link;
    struct option_args_t args;
    struct v6_monitor_event_t event[EVENT_MAX];
};

extern struct v6_monitor* create_v6_monitor(int argc, char *argv[]);
extern void cleanup_v6_monitor(struct v6_monitor* monitor);

#endif
