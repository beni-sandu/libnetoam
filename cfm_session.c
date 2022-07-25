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

/* Per thread variables */
__thread libnet_t *l;                                        /* libnet context */
__thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];             /* libnet error buffer */


/* Entry point of a new CFM LBM session */
void *cfm_session_run_lbm(void *args) {

    struct cfm_thread *current_thread = (struct cfm_thread *)args;
    struct cfm_session_params *current_params = current_thread->session_params;
    struct cfm_lb_pdu lbm_frame;
    uint8_t src_hwaddr[8];
    libnet_ptag_t eth_ptag = 0;
    uint8_t dst_hwaddr[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

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

    if (get_eth_mac(current_params->if_name, src_hwaddr) == -1){
        fprintf(stderr, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Build CFM common header for LMB frames */
    cfm_build_common_header(0, 0, CFM_OP_LBM, 0, 4, &lbm_frame.cfm_header);

    /* Build rest of LBM frame */
    cfm_build_lb_frame(12345678, 0, &lbm_frame);

    eth_ptag = libnet_build_ethernet(
        (uint8_t *)dst_hwaddr,                      /* Destination MAC */
        (uint8_t *)src_hwaddr,                      /* MAC of local interface */
        ETHERTYPE_CFM,                              /* Ethernet type */
        (uint8_t *)&lbm_frame,                      /* Payload (LBM frame filled above) */
        sizeof(lbm_frame),                          /* Payload size */
        l,                                          /* libnet handle */
        eth_ptag);                                  /* libnet tag */

    int c;

    pr_debug("CFM LBM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Send 3 identical frames to test */
    for (int i = 0; i < 3; i++)
        c = libnet_write(l);

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