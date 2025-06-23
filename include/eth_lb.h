/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ETH_LB_H
#define _ETH_LB_H

#include <net/ethernet.h>
#include <net/if.h>
#include <linux/limits.h>
#include <stdbool.h>

#include "oam_frame.h"

#define NET_NS_SIZE     (32U)
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
    char **dst_mac_list;                                        /* NULL terminated list of destination MAC addresses in string format (OAM_SESSION_LB_DISCOVER) */
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
    char log_file[PATH_MAX];                                    /* Log file path */
    bool is_multicast;                                          /* Flag for multicast sessions */
    bool enable_console_logs;                                   /* Output log messages to console too */
    bool log_utc;                                               /* Output log messages in UTC timezone */
};

/* ETH-LB session data */
struct oam_lb_session {
    uint32_t transaction_id;                                    /* Transaction identifier */
    int rx_sockfd;                                              /* RX socket file descriptor */
    int tx_sockfd;                                              /* TX socket file descriptor */
    struct timespec time_sent;                                  /* Time when the frame was sent */
    struct timespec time_received;                              /* Time when the frame was received */
    bool is_session_configured;                                 /* Flag for session configuration */
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
    uint8_t **dst_hwaddr_list;                                  /* List of destination MAC addresses in binary form */
    size_t dst_addr_count;                                      /* Number of destination MAC addresses from list */
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
void *oam_session_run_lb_discover(void *args);
void oam_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct oam_lb_pdu *oam_frame);

#endif //_ETH_LB_H