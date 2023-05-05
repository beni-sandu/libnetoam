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
void lb_session_cleanup(void *args);
ssize_t recvmsg_ppoll(int sockfd, struct msghdr *recv_hdr, int timeout_ms);
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
__thread struct cb_status callback_status;
__thread uint32_t lbm_missed_pings;
__thread uint32_t lbm_replied_pings;
__thread bool is_lbm_session_recovered;
__thread libnet_ptag_t eth_ptag = 0;
__thread cap_t caps;
__thread cap_flag_value_t cap_val;
__thread int ns_fd;
__thread char ns_buf[MAX_PATH] = "/run/netns/";

ssize_t recvmsg_ppoll(int sockfd, struct msghdr *recv_hdr, int timeout_ms)
{
    struct pollfd fds[1];
    struct timespec ts;
    int ret;

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = timeout_ms % 1000 * 1000000;

    ret = ppoll(fds, 1, &ts, NULL);

    if (ret == -1) {
        perror("ppoll"); //error in ppoll call
    }
    else if (ret == 0) {
        return -2; //timeout expired
    }
    else
        if (fds[0].revents & POLLIN)
            return recvmsg(sockfd, recv_hdr, 0);

    return EXIT_FAILURE;
}

/* Entry point of a new OAM LBM session */
void *oam_session_run_lbm(void *args)
{
    struct oam_thread *current_thread = (struct oam_thread *)args;
    struct oam_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    struct itimerspec tx_ts;
    struct sigevent tx_sev;
    struct oam_lb_session current_session;
    int if_index = 0;
    struct ether_header *eh;
    struct oam_lb_pdu *lbm_frame_p;

    /* Setup buffer and header structs for received packets */
    uint8_t recv_buf[8192];
	struct iovec recv_iov = {
          .iov_base = recv_buf,
          .iov_len = 8192,
    };

	union {
          struct cmsghdr cmsg;
          char buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
	} cmsg_buf;
	struct msghdr recv_hdr = {
          .msg_iov = &recv_iov,
          .msg_iovlen = 1,
          .msg_control = &cmsg_buf,
          .msg_controllen = sizeof(cmsg_buf)
     };

    /* Initialize some session and timer data */
    current_session.lbm_tx_timer = &tx_timer;
    current_session.frame = &lb_frame;
    current_session.eth_ptag = &eth_ptag;
    current_session.is_session_configured = false;
    current_session.send_next_frame = true;
    tx_timer.ts = &tx_ts;
    tx_timer.timer_id = NULL;
    tx_timer.is_timer_created = false;
    lbm_missed_pings = 0;
    lbm_replied_pings = 0;
    is_lbm_session_recovered = true;

    int flag_enable = 1;

    callback_status.cb_ret = OAM_CB_DEFAULT;
    callback_status.session_params = current_params;

    /* Install session cleanup handler */
    pthread_cleanup_push(lb_session_cleanup, (void *)&current_session);

    /* Check for CAP_NET_RAW capability */
    caps = cap_get_proc();
    if (caps == NULL) {
        perror("cap_get_proc");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_get_flag(caps, CAP_NET_RAW, CAP_EFFECTIVE, &cap_val) == -1) {
        perror("cap_get_flag");
        cap_free(caps);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_val != CAP_SET) {
        cap_free(caps);
        fprintf(stderr, "Execution requires CAP_NET_RAW capability.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* We don't need this anymore, so clean it */
    cap_free(caps);

    /* Configure network namespace */
    if (strlen(current_params->net_ns) != 0) {
        strcat(ns_buf, current_params->net_ns);

        ns_fd = open(ns_buf, O_RDONLY);

        if (ns_fd == -1) {
            perror("open ns fd");
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        if (setns(ns_fd, CLONE_NEWNET) == -1) {
            perror("set ns");
            close(ns_fd);
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        close(ns_fd);
    }

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
    current_session.l = l;

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
    oam_build_common_header(current_params->md_level, 0, OAM_OP_LBM, 0, 4, &lb_frame.oam_header);

    /* Build Ethernet header */

    /* If session is started on a VLAN, we ignore VLAN parameters, otherwise it will get double tagged */
    if (is_eth_vlan(current_params->if_name) == false) {

        /* If we a have priority code point or VLAN ID, we need to add a 802.1q header, even if VLAN ID is 0. */
        if (current_params->pcp > 0 || current_params->vlan_id > 0) {
            if (current_params->pcp > 7) {
                pr_debug("[%s] allowed PCP range is 0 - 7, setting to 0.\n", current_params->if_name);
                current_session.pcp = 0;
            } else {
                current_session.pcp = current_params->pcp;
            }
            current_session.vlan_id = current_params->vlan_id;

            eth_ptag = libnet_build_802_1q(
                (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
                (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                ETHERTYPE_VLAN,                                         /* Tag protocol identifier */
                current_session.pcp,                                    /* Priority code point */
                0x1,                                                    /* Drop eligible indicator(formerly CFI) */
                current_session.vlan_id,                                /* VLAN identifier */
                ETHERTYPE_OAM,                                          /* Protocol type */
                (uint8_t *)&lb_frame,                                   /* Payload (LBM frame filled above) */
                sizeof(lb_frame),                                       /* Payload size */
                l,                                                      /* libnet handle */
                eth_ptag);                                              /* libnet tag */
        } else {
            eth_ptag = libnet_build_ethernet(
                (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
                (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                ETHERTYPE_OAM,                                          /* Ethernet type */
                (uint8_t *)&lb_frame,                                   /* Payload (LBM frame filled above) */
                sizeof(lb_frame),                                       /* Payload size */
                l,                                                      /* libnet handle */
                eth_ptag);                                              /* libnet tag */
        }
    }

    /* Initial TX timer configuration */
    tx_sev.sigev_notify = SIGEV_THREAD;                         /* Notify via thread */
    tx_sev.sigev_notify_function = &lbm_timeout_handler;        /* Handler function */
    tx_sev.sigev_notify_attributes = NULL;                      /* Could be pointer to pthread_attr_t structure */
    tx_sev.sigev_value.sival_ptr = &current_session;            /* Pointer passed to handler */

    /* Configure TX interval */
    tx_ts.it_interval.tv_sec = current_params->interval_ms / 1000;
    tx_ts.it_interval.tv_nsec = current_params->interval_ms % 1000 * 1000000;
    tx_ts.it_value.tv_sec = current_params->interval_ms / 1000;
    tx_ts.it_value.tv_nsec = current_params->interval_ms % 1000 * 1000000;

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
    current_session.sockfd = sockfd;

    /* Make socket address reusable (TODO: check if this is needed for raw sockets, I suspect not) */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_enable, sizeof(flag_enable)) < 0) {
        fprintf(stderr, "Can't configure socket address to be reused.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if_index = if_nametoindex(current_params->if_name);
    if (if_index == 0) {
        perror("if_nametoindex");
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
    current_session.is_session_configured = true;

    pr_debug("LBM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Processing loop for incoming frames */
    while (true) {

        if (current_session.send_next_frame == true) {
            
            /* Update frame and send on wire */
            oam_build_lb_frame(current_session.transaction_id, 0, &lb_frame);
            oam_update_lb_frame(&lb_frame, &current_session, current_session.eth_ptag, l);
            int c = libnet_write(l);

            if (c == -1) {
                fprintf(stderr, "Write error: %s\n", libnet_geterror(l));
                continue;
            }

            current_session.send_next_frame = false;

            /* Get aprox timestamp of sent frame */
            clock_gettime(CLOCK_REALTIME, &(current_session.time_sent));
            pr_debug("Sent LBM with transaction id: %d\n", current_session.transaction_id);

            /* Reset timer */
            if (timer_settime(tx_timer.timer_id, 0, &tx_ts, NULL) == -1) {
                perror("timer settime");
                current_thread->ret = -1;
                sem_post(&current_thread->sem);
                pthread_exit(NULL);
            }
        
            /* Check our socket for a reply */
            numbytes = recvmsg_ppoll(sockfd, &recv_hdr, current_params->interval_ms);

            /* We didn't get any response in the expected interval */
            if (numbytes == -2) {
                lbm_missed_pings++;
                lbm_replied_pings = 0;
                is_lbm_session_recovered = false;
                printf("Request timeout for interface: %s, transaction_id: %d\n", current_params->if_name, current_session.transaction_id);
            }

            /* If we reached the missed pings threshold, use callback */
            if (current_params->missed_consecutive_ping_threshold > 0) {
                if (lbm_missed_pings == current_params->missed_consecutive_ping_threshold) {
                    if (current_params->callback != NULL) {
                        callback_status.cb_ret = OAM_CB_MISSED_PING_THRESH;
                        current_params->callback(&callback_status);
                    }

                    /* Reset counter */
                    lbm_missed_pings = 0;

                    /* If it is oneshot operation, close session */
                    if (current_params->is_oneshot == true)
                        pthread_exit(NULL);
                }
            }

            /* We got something, check data */
            if (numbytes > 0) {

                /* If it is not an OAM frame, drop it */
                eh = (struct ether_header *)recv_buf;
                if (ntohs(eh->ether_type) != ETHERTYPE_OAM)
                    continue;

                /* If frame is not addressed to this interface, drop it */
                if (memcmp(eh->ether_dhost, src_hwaddr, ETH_ALEN) != 0)
                    continue;
            
                /* Get aprox timestamp of received frame */
                clock_gettime(CLOCK_REALTIME, &current_session.time_received);

                /* If frame is not OAM LBR, discard it */
                lbm_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
                if (lbm_frame_p->oam_header.opcode != OAM_OP_LBR)
                    continue;
            
                /* We are receiving pings, reset missed counter */
                lbm_missed_pings = 0;
                lbm_replied_pings++;

                /* If we missed pings before, we are on a recovery path */
                if (current_params->ping_recovery_threshold > 0) {
                    if (is_lbm_session_recovered == false) {

                        /* We reached recovery threshold, use callback */
                        if (current_params->ping_recovery_threshold == lbm_replied_pings) {
                            is_lbm_session_recovered = true;
                            if (current_params->callback != NULL) {
                                callback_status.cb_ret = OAM_CB_RECOVER_PING_THRESH;
                                current_params->callback(&callback_status);
                            }
                        }
                    }
                }

                printf("Got LBR from: %02X:%02X:%02X:%02X:%02X:%02X trans_id: %d, time: %.3f ms\n", eh->ether_shost[0],
                        eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3], eh->ether_shost[4],eh->ether_shost[5],
                        ntohl(lbm_frame_p->transaction_id), ((current_session.time_received.tv_sec - current_session.time_sent.tv_sec) * 1000 +
                        (current_session.time_received.tv_nsec - current_session.time_sent.tv_nsec) / 1000000.0));     
            }
        }
    }

    pthread_cleanup_pop(0);

    /* Should never reach this */
    return NULL;
}

/* Entry point of a new OAM LBR session */
void *oam_session_run_lbr(void *args)
{
    struct oam_thread *current_thread = (struct oam_thread *)args;
    struct oam_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    int flag_enable = 1;
    int if_index;
    struct ether_header *eh;
    struct oam_lb_pdu *lbr_frame_p;
    int c;
    struct oam_lb_session current_session;

    /* Setup buffer and header structs for received packets */
    uint8_t recv_buf[8192];
	struct iovec recv_iov = {
          .iov_base = recv_buf,
          .iov_len = 8192,
    };

	union {
          struct cmsghdr cmsg;
          char buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
	} cmsg_buf;
	struct msghdr recv_hdr = {
          .msg_iov = &recv_iov,
          .msg_iovlen = 1,
          .msg_control = &cmsg_buf,
          .msg_controllen = sizeof(cmsg_buf)
     };

    /* Install session cleanup handler */
    pthread_cleanup_push(lb_session_cleanup, (void *)&current_session);

    /* Check for CAP_NET_RAW capability */
    caps = cap_get_proc();
    if (caps == NULL) {
        perror("cap_get_proc");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_get_flag(caps, CAP_NET_RAW, CAP_EFFECTIVE, &cap_val) == -1) {
        perror("cap_get_flag");
        cap_free(caps);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_val != CAP_SET) {
        cap_free(caps);
        fprintf(stderr, "Execution requires CAP_NET_RAW capability.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* We don't need this anymore, so clean it */
    cap_free(caps);

    /* Configure network namespace */
    if (strlen(current_params->net_ns) != 0) {
        strcat(ns_buf, current_params->net_ns);

        ns_fd = open(ns_buf, O_RDONLY);

        if (ns_fd == -1) {
            perror("open ns fd");
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        if (setns(ns_fd, CLONE_NEWNET) == -1) {
            perror("set ns");
            close(ns_fd);
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        close(ns_fd);
    }

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

    /* Copy libnet pointer */
    current_session.l = l;

    /* Create a raw socket for incoming frames */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("socket");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Copy socket fd */
    current_session.sockfd = sockfd;

    /* Make socket address reusable (TODO: check if this is needed for raw sockets, I suspect not) */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag_enable, sizeof(flag_enable)) < 0) {
        fprintf(stderr, "Can't configure socket address to be reused.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Enable packet auxdata */
    if (setsockopt(sockfd, SOL_PACKET, PACKET_AUXDATA, &flag_enable, sizeof(flag_enable)) < 0) {
        fprintf(stderr, "Can't enable packet auxdata.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if_index = if_nametoindex(current_params->if_name);
    if (if_index == 0) {
        perror("if_nametoindex");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Setup socket address */
    memset(&sll, 0, sizeof(struct sockaddr_ll));
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = if_index;
    sll.sll_protocol = htons(ETH_P_ALL);
    
    /* Bind it */
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1) {
        perror("bind");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Session configuration is successful, return a valid session id */
    current_session.is_session_configured = true;

    pr_debug("LBR session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Processing loop for incoming packets */
    while (true) {

        /* Wait for data on the socket */
        numbytes = recvmsg(sockfd, &recv_hdr, 0);

        /* We got something, look around */
        if (numbytes > 0) {

            pr_debug("Received frame on LBR session, %ld bytes.\n", numbytes);
            
            /* If frame has a tag, it is not for us */
            if (is_frame_tagged(&recv_hdr) == true)
                continue;
            
            /* If it is not an OAM frame, drop it */
            eh = (struct ether_header *)recv_buf;
            if (ntohs(eh->ether_type) != ETHERTYPE_OAM)
                continue;

            /* If frame is not addressed to this interface, drop it */
            if (memcmp(eh->ether_dhost, src_hwaddr, ETH_ALEN) != 0)
                continue;

            /* If frame is not OAM LBM, discard it */
            lbr_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
            if (lbr_frame_p->oam_header.opcode != OAM_OP_LBM)
                continue;

            /* Except for the LBR Opcode, all OAM specific PDU data is copied from the received LBM frame */
            memcpy(&lb_frame, lbr_frame_p, sizeof(struct oam_lb_pdu));
            lb_frame.oam_header.opcode = OAM_OP_LBR;

            /* Copy destination MAC address */
            memcpy(dst_hwaddr, eh->ether_shost, ETH_ALEN);

            /* Build ETH header for the LBR frame that we send as a reply */
            eth_ptag = libnet_build_ethernet(
                (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
                (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                ETHERTYPE_OAM,                                          /* Ethernet type */
                (uint8_t *)&lb_frame,                                   /* Payload (LBM frame filled above) */
                sizeof(lb_frame),                                       /* Payload size */
                l,                                                      /* libnet handle */
                eth_ptag);                                              /* libnet tag */
        
            /* Send OAM frame on wire */
            c = libnet_write(l);

            if (c == -1) {
                fprintf(stderr, "Write error: %s\n", libnet_geterror(l));
                continue;
            }
        }

    } // while (true)

    pthread_cleanup_pop(0);

    /* Should never reach this */
    return NULL;
}

/* 
 * Create a new OAM session, returns a session id
 * on successful creation, -1 otherwise
 */
oam_session_id oam_session_start(struct oam_session_params *params, enum oam_session_type session_type)
{    
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
            fprintf(stderr, "Invalid OAM session type.\n");
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

/* Stop a OAM session */
void oam_session_stop(oam_session_id session_id)
{
    if (session_id > 0) {
        pr_debug("Stopping OAM session: %ld\n", session_id);
        pthread_cancel(session_id);
        pthread_join(session_id, NULL);
    }
}

void lbm_timeout_handler(union sigval sv)
{
    struct oam_lb_session *current_session = sv.sival_ptr;

    current_session->transaction_id++;
    current_session->send_next_frame = true;
}

void lb_session_cleanup(void *args)
{    
    struct oam_lb_session *current_session = (struct oam_lb_session *)args;

    /* Cleanup timer data */
    if (current_session->lbm_tx_timer != NULL) {
        if (current_session->lbm_tx_timer->is_timer_created == true) {
            timer_delete(current_session->lbm_tx_timer->timer_id);
        
            /*
            * Temporary workaround for C++ programs, seems sometimes the timer doesn't 
            * get disarmed in time, and tries to use memory that was already freed.
            */
            usleep(100000);
        }
    }
    
    /* Clean up libnet context */
    if (current_session->l != NULL)
        libnet_destroy(current_session->l);
    
    /* Close socket */
    if (current_session->sockfd != 0)
        close(current_session->sockfd);

    /* 
     * If a session is not successfully configured, we don't call pthread_join on it,
     * only exit using pthread_exit. Calling pthread_detach here should automatically
     * release resources for unconfigured sessions.
     */
    if (current_session->is_session_configured == false)
        pthread_detach(pthread_self());
}