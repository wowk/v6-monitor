#ifndef EVENT_H__
#define EVENT_H__

#include "link.h"
#include <stdbool.h>


typedef enum  {
    EVENT_MIN,
    EVENT_NEW_ADDR = EVENT_MIN,
    EVENT_MORE_ADDR,
    EVENT_ADD_PREFIX,
    EVENT_NEW_PREFIX,
    EVENT_DEL_ADDR,
    EVENT_LOST_ADDR,
    EVENT_DEL_LINK,
    EVENT_LINK_RUNNING,
    EVENT_LINK_STOP,
    EVENT_DHCP6_MODIFIED,
    EVENT_DHCP6_RECONFIG,
    EVENT_RADVD_DISABLE,
    EVENT_RADVD_ENABLE,
    EVENT_ND_PROXY_ENABLE,
    EVENT_ND_PROXY_DISABLE,
    EVENT_SWITCH_TO_PD,
    EVENT_SWITCH_TO_RA,
    EVENT_SWITCH_TO_PT,
    EVENT_MODE_MODIFIED,
    EVENT_MAX,
} v6_monitor_event_code_t;

struct v6_monitor_event_t {
    v6_monitor_event_code_t code;
    bool triggerred;
    void* event_arg;
};

struct v6_monitor;

extern bool event_happened(struct v6_monitor* v6_monitor, v6_monitor_event_code_t ev_code);
extern void set_event(struct v6_monitor* v6_monitor, v6_monitor_event_code_t ev_code, void* arg);
extern void clear_event(struct v6_monitor* v6_monitor, v6_monitor_event_code_t ev_code);
extern void clear_all_event(struct v6_monitor *v6_monitor);
extern int handle_event(struct v6_monitor* v6_monitor);

#endif //EVENT_H__
