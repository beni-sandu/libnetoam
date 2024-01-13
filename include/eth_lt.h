/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ETH_LT_H
#define _ETH_LT_H

#include <stdbool.h>
#include <libnet.h>

#include "oam_session.h"

struct oam_ltm_session_params {
    char if_name[IF_NAME_SIZE];                                 /* Network interface name */
    char dst_mac[ETH_STR_LEN];                                  /* Destination MAC address in string format */
    void (*callback)(struct cb_status *status);                 /* Callback function */
    char net_ns[NET_NS_SIZE];                                   /* Network namespace */
    uint8_t meg_level;                                          /* Maintenance entity group level */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level (from 802.1q header) */
    uint8_t ttl;                                                /* Frame TTL */
    char log_file[MAX_PATH];                                    /* Path to log file */
};

struct oam_lt_session {
    uint8_t *src_mac;
    uint8_t *dst_mac;
    uint32_t transaction_id;
    bool is_session_configured;
    libnet_t *l;                                                /* libnet context */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level */
    uint8_t meg_level;
    struct oam_ltm_session_params *current_params;
    uint8_t ttl;
};

void *oam_session_run_ltm(void *args);
void *oam_session_run_ltr(void *args);

#endif //_ETH_LT_H