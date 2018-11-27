#ifndef SAL_INFO_H__
#define SAL_INFO_H__

struct link_t;
struct v6_monitor;

struct sal_info_t {
    char* dns;
    char* prefix;
    char* ip1;
    char* ip2;
    char* prefixlen;
    char* phone_number;
	char* dhcpserver;
	char* additional_num;
	char* sip;
	char* fqdn;
	char* ula_prefix;
	char* wanip;
	char* sip_name;
	char* sip_domain;
    char* ntp;
    char* domain;
    int v6_lan_mode;
    int old_v6_lan_mode;
};

extern int init_sal_info(struct sal_info_t* si);
extern int handle_sal_event(struct v6_monitor* v6_monitor);

#endif

