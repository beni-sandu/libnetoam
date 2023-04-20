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

#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>
#include <libnet.h>
#include <sys/capability.h>
#include <linux/if_packet.h>
#include <poll.h>

#include "oam_session.h"
#include "oam_frame.h"
#include "libnetoam.h"

/* Forward declarations */
void lbm_timeout_handler(union sigval sv);
int oam_update_timer(int interval, struct itimerspec *ts, struct oam_lbm_timer *timer_data);
void lbm_session_cleanup(void *args);
ssize_t recvfrom_ppoll(int sockfd, uint8_t *recv_buf, int buf_size, int timeout_ms);
void *oam_session_run_lbr(void *args);
void *oam_session_run_lbm(void *args);

/* Per thread variables */
__thread libnet_t *l;                                       /* libnet context */
__thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];            /* libnet error buffer */
__thread struct oam_lbm_timer tx_timer;                     /* TX timer */
__thread struct oam_lb_pdu lb_frame;
__thread int sockfd;                                        /* RX socket file descriptor */
__thread struct sockaddr_ll sll;                            /* RX socket address */
__thread ssize_t numbytes;                                  /* Number of bytes received */
__thread uint8_t recv_buf[ETH_DATA_LEN];                    /* Buffer for incoming frames */

ssize_t recvfrom_ppoll(int sockfd, uint8_t *recv_buf, int buf_size, int timeout_ms) {

    struct pollfd fds[1];
    struct timespec ts;
    int ret;

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = timeout_ms % 1000 * 1000;

    ret = ppoll(fds, 1, &ts, NULL);

    if (ret == -1) {
        perror("ppoll"); //error in ppoll call
    }
    else if (ret == 0) {
        return -2; //timeout expired
    }
    else
        if (fds[0].revents & POLLIN)
            return recvfrom(sockfd, recv_buf, buf_size, 0, NULL, NULL);
    
    return EXIT_FAILURE;
}

/* Entry point of a new oam LBM session */
void *oam_session_run_lbm(void *args) {

    struct oam_thread *current_thread = (struct oam_thread *)args;
    struct oam_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    libnet_ptag_t eth_ptag = 0;
    struct itimerspec tx_ts;
    struct sigevent tx_sev;
    struct oam_lb_session current_session;
    int if_index;
    struct ether_header *eh;
    struct oam_lb_pdu *lbm_frame_p;

    /* Initialize timer data */
    tx_timer.session_params = current_params;
    tx_timer.current_session = &current_session;
    tx_timer.frame = &lb_frame;
    tx_timer.eth_ptag = &eth_ptag;
    tx_timer.ts = &tx_ts;
    tx_timer.timer_id = NULL;
    tx_timer.is_timer_created = false;
    tx_timer.is_session_configured = false;

    int flag_enable = 1;

    /* Install session cleanup handler */
    pthread_cleanup_push(lbm_session_cleanup, (void *)&tx_timer);

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

    /* Build oam common header for LMB frames */
    oam_build_common_header(0, 0, OAM_OP_LBM, 0, 4, &lb_frame.oam_header);

    /* Build rest of the initial LBM frame */
    oam_build_lb_frame(current_session.transaction_id, 0, &lb_frame);

    eth_ptag = libnet_build_ethernet(
        (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
        (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
        ETHERTYPE_OAM,                                          /* Ethernet type */
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

    /* Create a raw socket for incoming frames */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_OAM))) == -1) {
        perror("socket");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Store the sockfd so we can close it from the cleanup handler */
    tx_timer.current_session->sockfd = sockfd;

    /* Make socket address reusable */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_enable, sizeof(flag_enable)) < 0) {
        fprintf(stderr, "Can't configure socket address to be reused.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if (get_eth_index(current_params->if_name, &if_index) == -1) {
        fprintf(stderr, "Can't get interface index.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Setup socket address */
    memset(&sll, 0, sizeof(struct sockaddr_ll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_index;
    sll.sll_protocol = htons(ETHERTYPE_OAM);
    
    /* Bind it */
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Session configuration is successful, return a valid session id */
    tx_timer.is_session_configured = true;
    pr_debug("LBM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Start sending LBM frames */
    if (timer_settime(tx_timer.timer_id, 0, &tx_ts, NULL) == -1) {
        perror("timer settime");
        current_thread->ret = -1;
        pthread_exit(NULL);
    }

    /* Processing loop for incoming frames */
    while (true) {
        
        /* Check our socket for data */
        numbytes = recvfrom_ppoll(sockfd, recv_buf, ETH_DATA_LEN, current_params->interval_ms);

        /* We didn't get any response in the expected interval */
        if (numbytes == -2)
            printf("Request timeout for interface: %s, transaction_id: %d\n", current_params->if_name, current_session.transaction_id);

        if (numbytes > 0) {
            eh = (struct ether_header *)recv_buf;
            
            /* Get aprox timestamp of received frame */
            clock_gettime(CLOCK_REALTIME, &current_session.time_received);

            /* Check destination MAC just in case */
            if (!(eh->ether_dhost[0] == src_hwaddr[0] &&
                    eh->ether_dhost[1] == src_hwaddr[1] &&
                    eh->ether_dhost[2] == src_hwaddr[2] &&
                    eh->ether_dhost[3] == src_hwaddr[3] &&
                    eh->ether_dhost[4] == src_hwaddr[4] &&
                    eh->ether_dhost[5] == src_hwaddr[5])) {
                pr_debug("Destination MAC of received oam frame is for a different interface.\n");
                continue;
            }

            /* If frame is not oam LBR, discard it */
            lbm_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
            if (lbm_frame_p->oam_header.opcode != OAM_OP_LBR)
                continue;

            printf("Got LBR from: %02X:%02X:%02X:%02X:%02X:%02X trans_id: %d, time: %.3f ms\n", eh->ether_shost[0],
                    eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3], eh->ether_shost[4],eh->ether_shost[5],
                    ntohl(lbm_frame_p->transaction_id), ((current_session.time_received.tv_sec - current_session.time_sent.tv_sec) * 1000 +
                    (current_session.time_received.tv_nsec - current_session.time_sent.tv_nsec) / 1000000.0));     
        }
    }

    pthread_cleanup_pop(0);

    /* Should never reach this */
    return NULL;
}

/* Entry point of a new oam LBR session */
void *oam_session_run_lbr(void *args) {

    struct oam_thread *current_thread = (struct oam_thread *)args;
    struct oam_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    int flag_enable = 1;
    int if_index;
    struct ether_header *eh;
    struct oam_lb_pdu *lbr_frame_p;
    libnet_ptag_t eth_ptag = 0;
    int c;


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

    /* Get source MAC address */
    if (get_eth_mac(current_params->if_name, src_hwaddr) == -1) {
        fprintf(stderr, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Create a raw socket for incoming frames */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_OAM))) == -1) {
        perror("socket");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Make socket address reusable */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_enable, sizeof(flag_enable)) < 0) {
        fprintf(stderr, "Can't configure socket address to be reused.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if (get_eth_index(current_params->if_name, &if_index) == -1) {
        fprintf(stderr, "Can't get interface index.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Setup socket address */
    memset(&sll, 0, sizeof(struct sockaddr_ll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_index;
    sll.sll_protocol = htons(ETHERTYPE_OAM);
    
    /* Bind it */
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    pr_debug("LBR session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Processing loop for incoming packets */
    while (true) {

        /* Wait for data on the socket */
        numbytes = recvfrom(sockfd, recv_buf, ETH_DATA_LEN, 0, NULL, NULL);

        /* We got something, look around */
        pr_debug("Received frame on LBR session, %ld bytes.\n", numbytes);
        eh = (struct ether_header *)recv_buf;

        /* Check destination MAC just in case */
        if (!(eh->ether_dhost[0] == src_hwaddr[0] &&
                    eh->ether_dhost[1] == src_hwaddr[1] &&
                    eh->ether_dhost[2] == src_hwaddr[2] &&
                    eh->ether_dhost[3] == src_hwaddr[3] &&
                    eh->ether_dhost[4] == src_hwaddr[4] &&
                    eh->ether_dhost[5] == src_hwaddr[5])) {
            pr_debug("Destination MAC of received oam frame is for a different interface.\n");
            continue;
        }

        /* If frame is not oam LBM, discard it */
        lbr_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
        if (lbr_frame_p->oam_header.opcode != OAM_OP_LBM)
            continue;

        /* Except for the LBR Opcode, all oam specific PDU data is copied from the received LBM frame */
        memcpy(&lb_frame, lbr_frame_p, sizeof(struct oam_lb_pdu));
        lb_frame.oam_header.opcode = OAM_OP_LBR;

        /* Copy destination MAC address */
        for (int i = 0; i < ETH_ALEN; i++)
            dst_hwaddr[i] = eh->ether_shost[i];

        /* Build everything for the LBR frame that we send as a reply */
        eth_ptag = libnet_build_ethernet(
            (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
            (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
            ETHERTYPE_OAM,                                          /* Ethernet type */
            (uint8_t *)&lb_frame,                                   /* Payload (LBM frame filled above) */
            sizeof(lb_frame),                                       /* Payload size */
            l,                                                      /* libnet handle */
            eth_ptag);                                              /* libnet tag */
        
        /* Send oam frame on wire */
        c = libnet_write(l);

        if (c == -1) {
            fprintf(stderr, "Write error: %s\n", libnet_geterror(l));
            continue;
        }

    } // while (true)

    return NULL;
}

/* 
 * Create a new oam session, returns a session id
 * on successful creation, -1 otherwise
 */
oam_session_id oam_session_start(struct oam_session_params *params, enum oam_session_type session_type) {
    
    pthread_t session_id;
    int ret;
    struct oam_thread new_thread;

    new_thread.session_params = params;
    new_thread.ret = 0;

    sem_init(&new_thread.sem, 0, 0);

    switch (session_type) {
        case OAM_SESSION_LBM:
            ret = pthread_create(&session_id, NULL, oam_session_run_lbm, (void *)&new_thread);
            break;
        case OAM_SESSION_LBR:
            ret = pthread_create(&session_id, NULL, oam_session_run_lbr, (void *)&new_thread);
            break;
        default:
            fprintf(stderr, "Invalid oam session type.\n");
    }

    if (ret) {
        fprintf(stderr, "oam_session_start for interface: %s failed, err: %d\n", params->if_name, ret);
        return -1;
    }

    sem_wait(&new_thread.sem);

    if (new_thread.ret != 0)
        return new_thread.ret;
    
    return session_id;
}

/* Stop a oam session */
void oam_session_stop(oam_session_id session_id) {

    if (session_id > 0) {
        pr_debug("Stopping oam session: %ld\n", session_id);
        pthread_cancel(session_id);
        pthread_join(session_id, NULL);
    }
}

void lbm_timeout_handler(union sigval sv) {

    struct oam_lbm_timer *timer_data = sv.sival_ptr;
    int c;
    struct oam_lb_pdu *lbm_frame = timer_data->frame;
    libnet_ptag_t *eth_tag = timer_data->eth_ptag;
    libnet_t *l = timer_data->l;
    struct oam_lb_session *current_session = timer_data->current_session;

    /* Update transaction id */
    oam_build_lb_frame(current_session->transaction_id, 0, lbm_frame);

    /* Update rest of the frame */
    oam_update_lb_frame(lbm_frame, current_session, eth_tag, l);

    /* Send oam frame on wire */
    c = libnet_write(l);

    if (c == -1) {
        fprintf(stderr, "Write error: %s\n", libnet_geterror(l));
        pthread_exit(NULL);
    }

    /* Get aprox timestamp of sent frame */
    clock_gettime(CLOCK_REALTIME, &current_session->time_sent);
    pr_debug("Sent LBM with transaction id: %d\n", current_session->transaction_id);

    current_session->transaction_id++;
}

void lbm_session_cleanup(void *args) {
    
    struct oam_lbm_timer *timer = (struct oam_lbm_timer *)args;

    /* Cleanup timer data */
    if (timer->is_timer_created == true) {
        timer_delete(timer->timer_id);
        
        /*
         * Temporary workaround for C++ programs, seems sometimes the timer doesn't 
         * get disarmed in time, and tries to use memory that was already freed.
         */
        usleep(100000);
    }
    
    /* Clean up libnet context */
    if (timer->l != NULL)
        libnet_destroy(timer->l);
    
    /* Close socket */
    if (timer->current_session->sockfd != 0)
        close(timer->current_session->sockfd);

    /* 
     * If a session is not successfully configured, we don't call pthread_join on it,
     * only exit using pthread_exit. Calling pthread_detach here should automatically
     * release resources for unconfigured sessions.
     */
    if (timer->is_session_configured == false)
        pthread_detach(pthread_self());
}