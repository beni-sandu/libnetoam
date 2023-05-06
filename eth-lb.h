/*
 * Copyright (C) 2022 Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    uint8_t md_level;                                           /* Maintenance domain level */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level (from 802.1q header) */
    char log_file[MAX_PATH];
};

struct oam_lb_session {
    uint8_t *src_mac;
    uint8_t *dst_mac;
    uint32_t transaction_id;
    int sockfd;
    struct timespec time_sent;
    struct timespec time_received;
    bool is_session_configured;
    struct oam_lb_pdu *frame;                                   /* Pointer to lbm frame */
    libnet_ptag_t *eth_ptag;                                    /* Pointer to libnet ETH tag */
    libnet_t *l;                                                /* libnet context */
    struct oam_lbm_timer *lbm_tx_timer;                         /* Pointer to LBM tx timer */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level */
    volatile bool send_next_frame;

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