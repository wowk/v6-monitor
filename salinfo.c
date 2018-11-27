#include "link.h"
#include "debug.h"
#include "event.h"
#include "monitor.h"
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <sal/sal_wan.h>


static void free_sal_info(struct sal_info_t* p)
{
    if( p->dns )
        free(p->dns);
    if( p->ntp )
        free(p->ntp);
    if( p->prefix )
        free(p->prefix);
    if(p->prefixlen)
        free(p->prefixlen);
    if(p->phone_number)
        free(p->phone_number);
    if(p->additional_num)
        free(p->additional_num);
    if(p->sip)
       free(p->sip);
    if(p->fqdn)
        free(p->fqdn);
    if(p->ula_prefix)
        free(p->ula_prefix);
    if(p->wanip)
        free(p->wanip);
    if(p->sip_name)
        free(p->sip_name);
    if(p->sip_domain)
        free(p->sip_domain);
    if(p->domain)
        free(p->domain);
}

static int update_sal_info(struct link_t* link, struct sal_info_t* newinfo)
{
	struct sal_info_t* p = &link->sal_info;

    free_sal_info(p);

	p->dns		= strdup(newinfo->dns);
	p->ntp		= strdup(newinfo->ntp);
	p->prefix	= strdup(newinfo->prefix);
	p->prefixlen= strdup(newinfo->prefixlen);
	p->phone_number	= strdup(newinfo->phone_number);
	p->additional_num = strdup(newinfo->additional_num);
	p->sip		= strdup(newinfo->sip);
	p->fqdn		= strdup(newinfo->fqdn);
	p->ula_prefix	= strdup(newinfo->ula_prefix);
	p->wanip		= strdup(newinfo->wanip);
	p->sip_name		= strdup(newinfo->sip_name);
	p->sip_domain	= strdup(newinfo->sip_domain);
	p->domain	= strdup(newinfo->domain);
}

static int safe_strcmp(const char* s1, const char* s2)
{
	if( s1 && s2 && 0 == strcmp(s1, s2) ){
		return 0;
	}else if( !s1 & !s2){
		return 0;
	}
	return 1;
}

static int cmp_dhcp6_info(struct sal_info_t* si1, struct sal_info_t* si2)
{
	if( safe_strcmp(si1->dns, si2->dns) ){
		debug("changed: dns : %s => %s", si1->dns, si2->dns);
		return 1;
	}
	if( safe_strcmp(si1->ntp, si2->ntp) ){
		debug("changed: ntp : %s => %s", si1->ntp, si2->ntp);
		return 1;
	}
	if( safe_strcmp(si1->sip, si2->sip) ){
		debug("changed: sip : %s => %s", si1->sip, si2->sip);
		return 1;
	}
	if( safe_strcmp(si1->fqdn, si2->fqdn) ){
		debug("changed: fqdn : %s => %s", si1->fqdn, si2->fqdn);
		return 1;
	}
	if( safe_strcmp(si1->wanip, si2->wanip) ){
		debug("changed: wanip : %s => %s", si1->wanip, si2->wanip);
		return 1;
	}
	if( safe_strcmp(si1->domain, si2->domain) ){
		debug("changed: domain : %s => %s", si1->domain, si2->domain);
		return 1;
	}
	if( safe_strcmp(si1->prefix, si2->prefix) ){
		debug("changed: prefix : %s => %s", si1->prefix, si2->prefix);
		return 1;
	}
	if( safe_strcmp(si1->sip_name, si2->sip_name) ){
		debug("changed: sip name : %s => %s", si1->sip_name, si2->sip_name);
		return 1;
	}
	if( safe_strcmp(si1->prefixlen, si2->prefixlen) ){
		debug("changed: prefixlen : %s => %s", si1->prefixlen, si2->prefixlen);
		return 1;
	}
	if( safe_strcmp(si1->ula_prefix, si2->ula_prefix) ){
		debug("changed: ULA prefix : %s => %s", si1->ula_prefix, si2->ula_prefix);
		return 1;
	}
	if( safe_strcmp(si1->sip_domain, si2->sip_domain) ){
		debug("changed: sip domain : %s => %s", si1->sip_domain, si2->sip_domain);
		return 1;
	}
	if( safe_strcmp(si1->phone_number, si2->phone_number) ){
		debug("changed: phone number : %s => %s", si1->phone_number, si2->phone_number);
		return 1;
	}
	if( safe_strcmp(si1->additional_num, si2->additional_num) ){
		debug("changed: additional_num : %s => %s", si1->additional_num, si2->additional_num);
		return 1;
	}
	return 0; 
}

static int load_sal_info(int wanid, struct sal_info_t* si)
{
	si->dns		= sal_wan_get_ipv6_dns_t(wanid) ?: "";
	si->ntp		= sal_wan_get_ipv6_ntp_t(wanid) ?: "";
	si->sip		= sal_wan_get_ipv6_sip_t(wanid) ?: "";
	si->fqdn	= sal_wan_get_ipv6_vi_fqdn(wanid) ?: "";
	si->wanip	=  sal_wan_get_ipv6_wan_ip_t(wanid) ?: "";
	si->domain	= sal_wan_get_ipv6_domain_t(wanid) ?: "";
	si->prefix	= sal_wan_get_ipv6_prefix_t(wanid) ?: "";
	si->sip_name	= sal_wan_get_ipv6_sip_name_t(wanid) ?: "";
	si->prefixlen	= sal_wan_get_ipv6_prefix_len_t(wanid) ?: "";
	si->ula_prefix	= sal_wan_get_ipv6_ula_prefix_t(wanid) ?: "";
	si->sip_domain	= sal_wan_get_ipv6_vi_sip_domain(wanid) ?: "";
	si->phone_number= sal_wan_get_ipv6_vi_phone_num(wanid) ?: "";
	si->additional_num= sal_wan_get_ipv6_vi_additional_num(wanid) ?: "";
	
	si->v6_lan_mode = sal_wan_get_boot_order_mode_t(wanid)[0] - '0';

	return 0;
}

int init_sal_info(struct sal_info_t* si)
{
	si->dns = strdup("");
	si->ntp = strdup("");
	si->sip = strdup("");
	si->fqdn = strdup("");
	si->wanip = strdup("");
	si->domain = strdup("");
	si->prefix = strdup("");
	si->sip_name = strdup("");
	si->prefixlen = strdup("");
	si->ula_prefix = strdup("");
	si->sip_domain = strdup("");
	si->phone_number = strdup("");
	si->additional_num = strdup("");
	si->v6_lan_mode = 0xff;
	si->old_v6_lan_mode = 0xff;

	return 0;
}

int handle_sal_event(struct v6_monitor* v6_monitor)
{
    bool modified = false;
    size_t len = 0;
    ssize_t retlen = 0;
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    retlen = read(fd, buffer, sizeof(buffer));
    if ( 0 > retlen ) {
        if( errno != EINTR ){
            error(errno, "failed to read inode event message");
            return -errno;
        }else{
            return 0;
        }
    }

    event = (struct inotify_event*)buffer;
    len = sizeof(struct inotify_event) + event->len;
    while ( retlen >= len ) {
        if ( event->mask & IN_CLOSE_WRITE) {
            modified = true;
            break;
        }
        retlen -= (sizeof(struct inotify_event) + event->len);
        event ++;
    }

    if ( modified ){
		bool changed = false;
		struct sal_info_t si;
        load_sal_info(v6_monitor->link.wan_id, &si);

        if( cmp_dhcp6_info(&v6_monitor->link.sal_info, &si) ){
			changed = true;
            set_event(v6_monitor, EVENT_DHCP6_MODIFIED, 0);
		}

		update_sal_info(link, &si);
    }

    return 0;
}
