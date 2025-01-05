/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ETH_LB_H
#define _ETH_LB_H

#include <stdbool.h>
#include <libnet.h>

#define IF_NAME_SIZE 32
#define NET_NS_SIZE 32
#define MAX_PATH 512
#define ETH_STR_LEN 18

struct oam_lb_session_params {
    char if_name[IF_NAME_SIZE];                                 /* Network interface name */
    char dst_mac[ETH_STR_LEN];                                  /* Destination MAC address in string format */
    uint32_t interval_ms;                                       /* Ping interval in miliseconds */
    uint32_t missed_consecutive_ping_threshold;                 /* Counter for consecutive missed pings */
    uint32_t ping_recovery_threshold;                           /* Recovery threshold counter */
    bool is_oneshot;                                            /* Flag for oneshot operation */
    void (*callback)(struct cb_status *status);                 /* Callback function */
    char net_ns[NET_NS_SIZE];                                   /* Network namespace name */
    uint8_t meg_level;                                          /* Maintenance entity group level */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level (from 802.1q header) */
    bool dei;                                                   /* Drop eligible indicator */
    char log_file[MAX_PATH];
    bool is_multicast;                                          /* Flag for multicast sessions */
    bool enable_console_logs;                                   /* Output log messages to console too */
};

struct oam_lb_session {
    uint32_t transaction_id;
    int sockfd;
    struct timespec time_sent;
    struct timespec time_received;
    bool is_session_configured;
    libnet_t *l;                                                /* libnet context */
    struct oam_lbm_timer *lbm_tx_timer;                         /* Pointer to LBM tx timer */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level */
    volatile bool send_next_frame;
    bool dei;                                                   /* Drop eligible indicator */
    bool is_frame_multicast;
    uint32_t interval_ms;
    bool is_multicast;
    uint8_t meg_level;
    bool custom_vlan;
    bool is_if_tagged;                                          /* Flag describing if session is started on a VLAN */
    bool enable_console_logs;                                   /* Output log messages to console too */
};

/* Data passed to per session timer */
struct oam_lbm_timer {
    bool is_timer_created;
    timer_t timer_id;                                           /* POSIX interval timer id */
    struct itimerspec *ts;
};

void *oam_session_run_lbm(void *args);
void *oam_session_run_lbr(void *args);

#endif //_ETH_LB_H