/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <libnet.h>
#include <sys/capability.h>

#include "../include/oam_session.h"
#include "../include/oam_frame.h"
#include "../include/eth_lt.h"

/* Forward declarations */
void *oam_session_run_ltm(void *args);
void *oam_session_run_ltr(void *args);

/* Per thread variables */
static __thread libnet_t *l;
static __thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];
static __thread libnet_ptag_t eth_ptag = 0;
static __thread cap_t caps;
static __thread cap_flag_value_t cap_val;
static __thread int ns_fd;
static __thread char ns_buf[MAX_PATH] = "/run/netns/";

void *oam_session_run_ltm(void *args)
{
    struct oam_session_thread *current_thread = (struct oam_session_thread *)args;
    struct oam_ltm_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    struct oam_ltm_pdu ltm_tx_frame;
    struct oam_ltm_pdu *ltm_frame_p;
    struct oam_lt_session current_session;
    const uint8_t eth_bcast_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    /* Initialize session data */
    current_session.l = NULL;
    current_session.current_params = current_params;
    current_session.ttl = current_params->ttl;
    current_session.meg_level = current_params->meg_level;

    l = libnet_init(
        LIBNET_LINK,                                /* injection type */
        current_params->if_name,                    /* network interface */
        libnet_errbuf);                             /* error buffer */

    if (l == NULL) {
        oam_pr_error(current_params->log_file, "libnet_init() failed: %s\n", libnet_errbuf);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Save libnet context */
    current_session.l = l;

    /* Get source MAC address */
    if (oam_get_eth_mac(current_params->if_name, src_hwaddr) == -1) {
        oam_pr_error(current_params->log_file, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get destination MAC address */
    if (oam_hwaddr_str2bin(current_params->dst_mac, dst_hwaddr) == -1) {
        oam_pr_error(current_params->log_file, "Error getting destination MAC address.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Seed random generator used for transaction id */
    srandom((uint64_t)current_params);
    current_session.transaction_id = random();

    /* Build oam common header for LTM frames (TODO: check HWOnly bit) */
    oam_build_common_header(current_session.meg_level, 0, OAM_OP_LTM, 0, 17, &ltm_tx_frame.oam_header);

    /* Build LTM frame */
    oam_build_ltm_frame(current_session.transaction_id, current_session.ttl, src_hwaddr, dst_hwaddr, 0, &ltm_tx_frame);

    /* Build Ethernet header */
    eth_ptag = libnet_build_ethernet(
                eth_bcast_addr,                                         /* Destination MAC */
                (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                ETHERTYPE_OAM,                                          /* Ethernet type */
                (uint8_t *)&ltm_tx_frame,                               /* Payload (LTM frame filled above) */
                sizeof(ltm_tx_frame),                                   /* Payload size */
                l,                                                      /* libnet context */
                eth_ptag);                                              /* libnet eth tag */
    
    if (eth_ptag == -1) {
        oam_pr_error(current_params->log_file, "Can't build LTM frame: %s\n", libnet_geterror(l));
        pthread_exit(NULL);
    }

    /* Session configuration successful, return a valid session ID */
    current_session.is_session_configured = true;
    oam_pr_debug(current_params->log_file, "LTM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Send 1 LTM frame on wire */
    if (libnet_write(l) == -1) {
        oam_pr_error(current_params->log_file, "Write error: %s\n", libnet_geterror(l));
        pthread_exit(NULL);
    }

    return NULL;
}

void *oam_session_run_ltr(void *args)
{
    return NULL;
}