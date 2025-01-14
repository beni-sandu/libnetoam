/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ETH_LB_H
#define _ETH_LB_H

#include <stdbool.h>
#include <libnet.h>
#include <net/ethernet.h>

#include "oam_frame.h"

#define NET_NS_SIZE     (32U)
#define MAX_PATH        (512U)
#define ETH_STR_LEN     (18U)

/*
 * ETH-LB PDU is similar for both LBM/LBR frames.
 *
 * There is an optional TLV that could be filled between transaction ID and end tlv,
 * that we currently don't include.
 */
struct oam_lb_pdu {
	struct oam_common_header oam_header;
	uint32_t transaction_id;
	struct oam_sender_id_tlv sender_id;
	uint8_t end_tlv;
} __attribute__((__packed__));

/* ETH-LB session parameters */
struct oam_lb_session_params {
    char if_name[IFNAMSIZ];                                     /* Network interface name */
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
    char log_file[MAX_PATH];                                    /* Log file path */
    bool is_multicast;                                          /* Flag for multicast sessions */
    bool enable_console_logs;                                   /* Output log messages to console too */
    bool log_utc;                                               /* Output log messages in UTC timezone */
};

/* ETH-LB session data */
struct oam_lb_session {
    uint32_t transaction_id;                                    /* Transaction identifier */
    int sockfd;                                                 /* Socket file descriptor */
    struct timespec time_sent;                                  /* Time when the frame was sent */
    struct timespec time_received;                              /* Time when the frame was received */
    bool is_session_configured;                                 /* Flag for session configuration */
    libnet_t *l;                                                /* libnet context */
    struct oam_lbm_timer *lbm_tx_timer;                         /* Pointer to LBM tx timer */
    uint16_t vlan_id;                                           /* VLAN identifier */
    uint8_t pcp;                                                /* Frame priority level */
    volatile bool send_next_frame;                              /* Flag for sending next frame */
    bool dei;                                                   /* Drop eligible indicator */
    bool is_frame_multicast;                                    /* Flag for multicast frames */
    uint32_t interval_ms;                                       /* Ping interval in miliseconds */
    bool is_multicast;                                          /* Flag for multicast sessions */
    uint8_t meg_level;                                          /* Maintenance entity group level */
    bool custom_vlan;                                           /* Flag for custom VLAN */
    bool is_if_tagged;                                          /* Flag describing if session is started on a VLAN */
    struct oam_lb_session_params *current_params;               /* Pointer to session parameters */
};

/* LBM session timer */
struct oam_lbm_timer {
    bool is_timer_created;                                      /* Flag for timer creation */
    timer_t timer_id;                                           /* POSIX interval timer id */
    struct itimerspec *ts;                                      /* Interval timer specification */
};

/* ETH-LB prototypes */
void *oam_session_run_lbm(void *args);
void *oam_session_run_lbr(void *args);
void oam_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct oam_lb_pdu *oam_frame);

#endif //_ETH_LB_H