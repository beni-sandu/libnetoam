/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MEP - MEG End Point (A point marking the end of an ETH MEG that is capable of initiating
 * and terminating OAM frames for fault management and performance monitoring. A MEP does not
 * add a new forwarding identifier to the transit ETH flows. A MEP does not terminate the transit
 * ETH flows, though it can observe these flows (e.g., count frames).)
 * 
 * MIP - MEG Intermediate Point ( An intermediate point in a MEG that is capable of reacting to some OAM frames.
 * A MIP does not initiate OAM frames. A MIP takes no action on the transit ETH flows.)
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
static void ltm_timeout_handler(union sigval sv);
static void ltm_session_cleanup(void *args);
static void ltr_session_cleanup(void *args);

/* Per thread variables */
static __thread libnet_t *l;
static __thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];
static __thread libnet_ptag_t eth_ptag = 0;
static __thread cap_t caps;
static __thread cap_flag_value_t cap_val;
static __thread int ns_fd;
static __thread char ns_buf[MAX_PATH] = "/run/netns/";
static __thread struct oam_ltm_timer tx_timer;

void *oam_session_run_ltm(void *args)
{
    struct oam_session_thread *current_thread = (struct oam_session_thread *)args;
    struct oam_ltm_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    struct oam_ltm_pdu ltm_tx_frame;
    struct oam_ltm_pdu *ltm_frame_p;
    struct oam_ltm_egress_id_tlv egress_id;
    struct oam_ltm_session current_session;
    const uint8_t eth_bcast_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    struct itimerspec tx_ts;
    struct sigevent tx_sev;

    /* Initialize session data */
    current_session.l = NULL;
    current_session.current_params = current_params;
    current_session.ttl = current_params->ttl;
    current_session.meg_level = current_params->meg_level;
    current_session.tx_timer_p = &tx_timer;
    current_session.is_session_configured = false;
    current_session.rx_sockfd = 0;
    current_session.send_next_frame = true;
    tx_timer.ts = &tx_ts;
    tx_timer.timer_id = NULL;
    tx_timer.is_timer_created = false;

    /* Install session cleanup handler */
    pthread_cleanup_push(ltm_session_cleanup, (void *)&current_session);

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

    /* Build OAM common header for LTM frame (TODO: check HWOnly bit) */
    oam_build_common_header(current_session.meg_level, 0, OAM_OP_LTM, 0, 17, &ltm_tx_frame.oam_header);

    /* Build LTM frame */
    egress_id.type = OAM_TLV_LTM_EGRESS;
    egress_id.length = ntohs(8);
    memset(&egress_id.zero_pad, 0, 2);
    memcpy(egress_id.source_mac, src_hwaddr, ETH_ALEN);
    oam_build_ltm_frame(current_session.transaction_id, current_session.ttl, src_hwaddr, dst_hwaddr, &egress_id, 0, &ltm_tx_frame);

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

    /* Initial TX timer configuration */
    tx_sev.sigev_notify = SIGEV_THREAD;                         /* Notify via thread */
    tx_sev.sigev_notify_function = &ltm_timeout_handler;        /* Handler function */
    tx_sev.sigev_notify_attributes = NULL;                      /* Could be pointer to pthread_attr_t structure */
    tx_sev.sigev_value.sival_ptr = &current_session;            /* Pointer passed to handler */

    /*
     * Standard says we send 1 LTM frame every 5 seconds.
     * Check if we should make this configurable, with 5s minimum interval.
     */
    tx_ts.it_interval.tv_sec = 5;
    tx_ts.it_interval.tv_nsec = 0;
    tx_ts.it_value.tv_sec = 5;
    tx_ts.it_value.tv_nsec = 0;

    /* Create TX timer */
    if (timer_create(CLOCK_MONOTONIC, &tx_sev, &(tx_timer.timer_id)) == -1) {
        oam_pr_error(current_params->log_file, "Cannot create timer.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }
    current_session.tx_timer_p->is_timer_created = true;

    /* Session configuration successful, return a valid session ID */
    current_session.is_session_configured = true;
    oam_pr_debug(current_params->log_file, "LTM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Start timer */
    if (timer_settime(tx_timer.timer_id, 0, &tx_ts, NULL) == -1) {
        oam_pr_error(current_params->log_file, "timer_settime.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    bool frame_sent = false;

    /* Main processing loop */
    while (true) {

        if (current_session.send_next_frame == true) {
            frame_sent = false;

            if (frame_sent == false) {
            
                /* Bump transaction id */
                current_session.transaction_id++;
            
                /* Update LTM frame */
                oam_build_ltm_frame(current_session.transaction_id, current_session.ttl, src_hwaddr, dst_hwaddr, &egress_id, 0, &ltm_tx_frame);
            
                /* Update Ethernet header */
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

                /* Send frame on wire */
                if (libnet_write(l) == -1) {
                    oam_pr_error(current_params->log_file, "Write error: %s\n", libnet_geterror(l));
                    current_session.send_next_frame = false;
                    continue;
                }

                current_session.send_next_frame = false;
                frame_sent = true;
            } // if (frame_sent == false)
        } // if (current_session.send_next_frame == true)
    } // while (true)

    pthread_cleanup_pop(0);

    return NULL;
}

void *oam_session_run_ltr(void *args)
{
    struct oam_session_thread *current_thread = (struct oam_session_thread *)args;
    struct oam_ltr_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    struct oam_ltr_pdu ltr_tx_frame;
    struct oam_ltr_pdu *ltr_frame_p;
    struct oam_ltm_pdu *ltm_frame_p;
    struct oam_ltr_session current_session;
    const uint8_t eth_bcast_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    int flag_enable = 1;
    int rx_if_index;
    struct sockaddr_ll rx_sll;
    ssize_t rx_bytes;
    struct ether_header *eh_p;

    /* Setup buffer and header structs for received frames */
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

    /* Initialize session data */
    current_session.ingress_l = NULL;
    current_session.current_params = current_params;
    current_session.meg_level = current_params->meg_level;
    current_session.is_session_configured = false;
    current_session.rx_sockfd = 0;

    /* Install session cleanup handler */
    pthread_cleanup_push(ltr_session_cleanup, (void *)&current_session);

    /* Create one RX socket which is used to listen for LTM PDUs. */
    if ((current_session.rx_sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        oam_pr_error(current_params->log_file, "Cannot create RX socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* We will probably need packet auxdata (TODO: remove if not needed) */
    if (setsockopt(current_session.rx_sockfd, SOL_PACKET, PACKET_AUXDATA, &flag_enable, sizeof(flag_enable)) < 0) {
        oam_pr_error(current_params->log_file, "Can't enable packet auxdata.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    rx_if_index = if_nametoindex(current_params->if_name);
    if (rx_if_index == 0) {
        oam_pr_error(current_params->log_file, "Error getting RX interface index.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Setup socket address */
    memset(&rx_sll, 0, sizeof(struct sockaddr_ll));
    rx_sll.sll_family = AF_PACKET;
    rx_sll.sll_ifindex = rx_if_index;
    rx_sll.sll_protocol = htons(ETH_P_ALL);
    
    /* Bind it */
    if (bind(current_session.rx_sockfd, (struct sockaddr *)&rx_sll, sizeof(rx_sll)) == -1) {
        oam_pr_error(current_params->log_file, "Cannot bind RX socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Session configuration successful, return a valid session ID */
    current_session.is_session_configured = true;
    oam_pr_debug(current_params->log_file, "LTR session configured successfully [%s].\n", current_params->if_name);
    sem_post(&current_thread->sem);

    /* Listen for incoming LTMs on interface */
    while (true) {

        /* Wait for data on the socket */
        rx_bytes = recvmsg(current_session.rx_sockfd, &recv_hdr, 0);

        /* We got something, look around */
        if (rx_bytes > 0) {
            
            /* Get ETH header */
            eh_p = (struct ether_header *)recv_buf;
            
            /* If it is not an OAM frame, drop it */
            if (ntohs(eh_p->ether_type) != ETHERTYPE_OAM)
                continue;

            oam_pr_debug(current_params->log_file, "Received OAM frame on LTR session, %ld bytes.\n", rx_bytes);

            /* If frame is not OAM LTM, discard it */
            ltm_frame_p = (struct oam_ltm_pdu *)(recv_buf + sizeof(struct ether_header));
            if (ltm_frame_p->oam_header.opcode != OAM_OP_LTM)
                continue;

            /* Check MEG level*/
            if (((ltm_frame_p->oam_header.byte1.meg_level >> 5) & 0x7) != current_session.meg_level) {
                oam_pr_debug(current_params->log_file, "Ignoring LTM with different MEG level: %d != %d\n", ((ltm_frame_p->oam_header.byte1.meg_level >> 5) & 0x7),
                            current_session.meg_level);
                continue;
            }

            /* If TTL is 0, drop it */
            if (ltm_frame_p->ttl == 0)
                continue;
        }
    } // while (true)

    pthread_cleanup_pop(0);

    return NULL;
}

static void ltm_session_cleanup(void *args)
{    
    struct oam_ltm_session *current_session = (struct oam_ltm_session *)args;

    /* Cleanup timer data */
    if (current_session->tx_timer_p != NULL) {
        if (current_session->tx_timer_p->is_timer_created == true) {
            timer_delete(current_session->tx_timer_p->timer_id);
        
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
    if (current_session->rx_sockfd != 0)
        close(current_session->rx_sockfd);

    /* 
     * If a session is not successfully configured, we don't call pthread_join on it,
     * only exit using pthread_exit. Calling pthread_detach here should automatically
     * release resources for unconfigured sessions.
     */
    if (current_session->is_session_configured == false)
        pthread_detach(pthread_self());
}

static void ltr_session_cleanup(void *args)
{    
    struct oam_ltr_session *current_session = (struct oam_ltr_session *)args;
    
    /* Clean up ingress libnet context */
    if (current_session->ingress_l != NULL)
        libnet_destroy(current_session->ingress_l);
    
    /* Close socket */
    if (current_session->rx_sockfd != 0)
        close(current_session->rx_sockfd);

    /* 
     * If a session is not successfully configured, we don't call pthread_join on it,
     * only exit using pthread_exit. Calling pthread_detach here should automatically
     * release resources for unconfigured sessions.
     */
    if (current_session->is_session_configured == false)
        pthread_detach(pthread_self());
}

static void ltm_timeout_handler(union sigval sv)
{
    struct oam_ltm_session *current_session = sv.sival_ptr;

    current_session->send_next_frame = true;
}