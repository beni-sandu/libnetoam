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

#include <pthread.h>
#include <stdio.h>
#include <libnet.h>
#include <sys/capability.h>

#include "cfm_session.h"
#include "cfm_frame.h"
#include "libnetcfm.h"

/* Forward declarations */
void lbm_timeout_handler(union sigval sv);
int cfm_update_timer(int interval, struct itimerspec *ts, struct cfm_lbm_timer *timer_data);

/* Per thread variables */
__thread libnet_t *l;                                       /* libnet context */
__thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];            /* libnet error buffer */
__thread struct cfm_lbm_timer tx_timer;                     /* TX timer */
__thread struct cfm_lb_pdu lb_frame;

/* Entry point of a new CFM LBM session */
void *cfm_session_run_lbm(void *args) {

    struct cfm_thread *current_thread = (struct cfm_thread *)args;
    struct cfm_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    libnet_ptag_t eth_ptag = 0;
    struct itimerspec tx_ts;
    struct sigevent tx_sev;
    struct cfm_lb_session current_session;

    /* Initialize timer data */
    tx_timer.session_params = current_params;
    tx_timer.current_session = &current_session;
    tx_timer.frame = &lb_frame;
    tx_timer.eth_ptag = &eth_ptag;
    tx_timer.ts = &tx_ts;
    tx_timer.timer_id = NULL;
    tx_timer.is_timer_created = false;
    tx_timer.is_session_configured = false;

    l = libnet_init(
        LIBNET_LINK,                                /* injection type */
        current_params->if_name,                    /* network interface */
        libnet_errbuf);                             /* error buffer */

    if (l == NULL) {
        fprintf(stderr, "libnet_init() failed: %s\n", libnet_errbuf);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Copy libnet context pointer */
    tx_timer.l = l;

    /* Get source MAC address */
    if (get_eth_mac(current_params->if_name, src_hwaddr) == -1) {
        fprintf(stderr, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get destination MAC address */
    if (hwaddr_str2bin(current_params->dst_mac, dst_hwaddr) == -1) {
        fprintf(stderr, "Error getting destination MAC address.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Save HW addresses to current session */
    current_session.src_mac = src_hwaddr;
    current_session.dst_mac = dst_hwaddr;

    /* Seed random generator used for transaction id */
    srandom((uint64_t)current_params);
    current_session.transaction_id = random();

    /* Build CFM common header for LMB frames */
    cfm_build_common_header(0, 0, CFM_OP_LBM, 0, 4, &lb_frame.cfm_header);

    /* Build rest of the initial LBM frame */
    cfm_build_lb_frame(current_session.transaction_id, 0, &lb_frame);

    eth_ptag = libnet_build_ethernet(
        (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
        (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
        ETHERTYPE_CFM,                                          /* Ethernet type */
        (uint8_t *)&lb_frame,                                   /* Payload (LBM frame filled above) */
        sizeof(lb_frame),                                       /* Payload size */
        l,                                                      /* libnet handle */
        eth_ptag);                                              /* libnet tag */

    /* Initial TX timer configuration */
    tx_sev.sigev_notify = SIGEV_THREAD;                         /* Notify via thread */
    tx_sev.sigev_notify_function = &lbm_timeout_handler;        /* Handler function */
    tx_sev.sigev_notify_attributes = NULL;                      /* Could be pointer to pthread_attr_t structure */
    tx_sev.sigev_value.sival_ptr = &tx_timer;                   /* Pointer passed to handler */

    /* Configure TX interval */
    tx_ts.it_interval.tv_sec = current_params->interval_ms / 1000;
    tx_ts.it_interval.tv_nsec = current_params->interval_ms % 1000 * 1000;
    tx_ts.it_value.tv_sec = current_params->interval_ms / 1000;
    tx_ts.it_value.tv_nsec = current_params->interval_ms % 1000 * 1000;

    /* Create TX timer */
    if (timer_create(CLOCK_REALTIME, &tx_sev, &(tx_timer.timer_id)) == -1) {
        perror("timer_create");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Timer should be created, but we still get a NULL pointer sometimes */
    tx_timer.is_timer_created = true;
    pr_debug("TX timer ID: %p\n", tx_timer.timer_id);

    /* Session configuration is successful, return a valid session id */
    tx_timer.is_session_configured = true;
    sem_post(&current_thread->sem);

    /* Start sending LBM frames */
    if (timer_settime(tx_timer.timer_id, 0, &tx_ts, NULL) == -1) {
        perror("timer settime");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Processing loop for incoming frames */
    while (true) {}

    return NULL;
}

/* Entry point of a new CFM LBR session */
void *cfm_session_run_lbr(void *args) {

    struct cfm_thread *current_thread = (struct cfm_thread *)args;
    struct cfm_session_params *current_params = current_thread->session_params;

    pr_debug("CFM LBR session configured successfully.\n");
    sem_post(&current_thread->sem);

    return NULL;
}

/* 
 * Create a new BFD session, returns a session id
 * on successful creation, -1 otherwise
 */
cfm_session_id cfm_session_start(struct cfm_session_params *params, enum cfm_session_type session_type) {
    
    pthread_t session_id;
    int ret;
    struct cfm_thread new_thread;

    new_thread.session_params = params;
    new_thread.ret = 0;

    sem_init(&new_thread.sem, 0, 0);

    switch (session_type) {
        case CFM_SESSION_LBM:
            ret = pthread_create(&session_id, NULL, cfm_session_run_lbm, (void *)&new_thread);
            break;
        case CFM_SESSION_LBR:
            ret = pthread_create(&session_id, NULL, cfm_session_run_lbr, (void *)&new_thread);
            break;
        default:
            fprintf(stderr, "Invalid CFM session type.\n");
    }

    if (ret) {
        fprintf(stderr, "cfm_session_create for interface: %s failed, err: %d\n", params->if_name, ret);
        return -1;
    }

    sem_wait(&new_thread.sem);

    if (new_thread.ret != 0)
        return new_thread.ret;
    
    return session_id;
}

/* Stop a CFM session */
void cfm_session_stop(cfm_session_id session_id) {

    if (session_id > 0) {
        pr_debug("Stopping CFM session: %ld\n", session_id);
        pthread_cancel(session_id);
        pthread_join(session_id, NULL);
    }
}

void lbm_timeout_handler(union sigval sv) {

    struct cfm_lbm_timer *timer_data = sv.sival_ptr;
    int c;
    struct cfm_lb_pdu *lbm_frame = timer_data->frame;
    libnet_ptag_t *eth_tag = timer_data->eth_ptag;
    libnet_t *l = timer_data->l;
    struct cfm_lb_session *current_session = timer_data->current_session;

    /* Update transaction id */
    cfm_build_lb_frame(current_session->transaction_id++, 0, lbm_frame);

    /* Update rest of the frame */
    cfm_update_lb_frame(lbm_frame, current_session, eth_tag, l);

    /* Send CFM frame on wire */
    c = libnet_write(l);

    if (c == -1) {
        fprintf(stderr, "Write error: %s\n", libnet_geterror(l));
        pthread_exit(NULL);
    }
}