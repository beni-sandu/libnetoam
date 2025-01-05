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
#include <linux/if_packet.h>
#include <poll.h>

#include "../include/oam_session.h"
#include "../include/oam_frame.h"
#include "../include/libnetoam.h"

/* Forward declarations */
static void lbm_timeout_handler(union sigval sv);
static void lb_session_cleanup(void *args);
static ssize_t recvmsg_ppoll(int sockfd, struct msghdr *recv_hdr, uint32_t timeout_ms);
void *oam_session_run_lbr(void *args);
void *oam_session_run_lbm(void *args);

/* Per thread variables */
static __thread libnet_t *l;                                       /* libnet context */
static __thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];            /* libnet error buffer */
static __thread struct oam_lbm_timer tx_timer;                     /* TX timer */
static __thread struct oam_lb_pdu lb_frame;
static __thread int sockfd;                                        /* RX socket file descriptor */
static __thread struct sockaddr_ll sll;                            /* RX socket address */
static __thread ssize_t numbytes;                                  /* Number of bytes received */
static __thread struct cb_status callback_status;
static __thread uint32_t lbm_missed_pings;
static __thread uint32_t lbm_replied_pings;
static __thread uint32_t lbm_multicast_replies;
static __thread bool is_lbm_session_recovered;
static __thread libnet_ptag_t eth_ptag = 0;
static __thread cap_t caps;
static __thread cap_flag_value_t cap_val;
static __thread int ns_fd;
static __thread char ns_buf[MAX_PATH] = "/run/netns/";
static __thread struct tpacket_auxdata recv_auxdata;

static ssize_t recvmsg_ppoll(int sockfd, struct msghdr *recv_hdr, uint32_t timeout_ms)
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
        oam_pr_error(NULL, "ppoll call error.\n"); //error in ppoll call
        return -1;
    } else if (ret == 0) {
        return -2; //timeout expired
    } else
        if (fds[0].revents & POLLIN)
            return recvmsg(sockfd, recv_hdr, 0);

    return -1;
}

/* Entry point of a new OAM LBM session */
void *oam_session_run_lbm(void *args)
{
    struct oam_session_thread *current_thread = (struct oam_session_thread *)args;
    struct oam_lb_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    struct itimerspec tx_ts;
    struct sigevent tx_sev;
    struct oam_lb_session current_session;
    int if_index = 0;
    struct ether_header *eh;
    struct oam_lb_pdu *lbm_frame_p;
    int flag_enable = 1;
    int ret = 0;

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
    current_session.is_session_configured = false;
    current_session.send_next_frame = true;
    current_session.interval_ms = current_params->interval_ms;
    current_session.is_multicast = false;
    current_session.meg_level = current_params->meg_level;
    current_session.custom_vlan = false;
    current_session.current_params = current_params;
    current_session.l = NULL;
    current_session.sockfd = 0;
    current_session.is_if_tagged = false;

    tx_timer.ts = &tx_ts;
    tx_timer.timer_id = NULL;
    tx_timer.is_timer_created = false;
    lbm_missed_pings = 0;
    lbm_replied_pings = 0;
    lbm_multicast_replies = 0;
    is_lbm_session_recovered = true;

    callback_status.cb_ret = OAM_LB_CB_DEFAULT;
    callback_status.session_params = current_params;

    /* Install session cleanup handler */
    pthread_cleanup_push(lb_session_cleanup, (void *)&current_session);

    /* Check for CAP_NET_RAW capability */
    caps = cap_get_proc();
    if (caps == NULL) {
        oam_pr_error(current_params->log_file, "cap_get_proc.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_get_flag(caps, CAP_NET_RAW, CAP_EFFECTIVE, &cap_val) == -1) {
        oam_pr_error(current_params->log_file, "cap_get_flag.\n");
        cap_free(caps);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_val != CAP_SET) {
        cap_free(caps);
        oam_pr_error(current_params->log_file, "Execution requires CAP_NET_RAW capability.\n");
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
            oam_pr_error(current_params->log_file, "open ns fd.\n");
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        if (setns(ns_fd, CLONE_NEWNET) == -1) {
            oam_pr_error(current_params->log_file, "set ns.\n");
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
        oam_pr_error(current_params->log_file, "libnet_init() failed: %s\n", libnet_errbuf);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Copy libnet context pointer */
    current_session.l = l;

    /* Get source MAC address */
    if (oam_get_eth_mac(current_params->if_name, src_hwaddr) == -1) {
        oam_pr_error(current_params->log_file, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get destination MAC address */
    if (current_params->is_multicast == true) {

        /* If session is multicast, we need to adjust some parameters */

        /* Set destination ETH address to broadcast */
        memset(dst_hwaddr, 0xff, ETH_ALEN);
        current_session.is_multicast = true;

        /* Thresholds and callback are not used during multicast */
        current_params->missed_consecutive_ping_threshold = 0;
        current_params->ping_recovery_threshold = 0;
        current_params->is_oneshot = false;
        current_params->callback = NULL;

        /* We should probably ignore VLAN headers too? */
        current_params->vlan_id = 0;
        current_params->pcp = 0;

        /* Standard says that interval should be 5s for ETH-LB multicast mode */
        if (current_session.interval_ms < 5000)
            current_session.interval_ms = 5000;

    } else if (oam_hwaddr_str2bin(current_params->dst_mac, dst_hwaddr) == -1) {
        oam_pr_error(current_params->log_file, "Error getting destination MAC address.\n");
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
    oam_build_common_header(current_session.meg_level, 0, OAM_OP_LBM, 0, 4, &lb_frame.oam_header);

    /* Check if interface is a VLAN */
    ret = oam_is_eth_vlan(current_params->if_name);
    if (ret == -1) {
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* If session is started on a VLAN, we ignore VLAN parameters, otherwise it will get double tagged */
    if (ret == 1) {

        /* If we a have priority code point or VLAN ID, we need to add a 802.1q header, even if VLAN ID is 0. */
        if (current_params->pcp > 0 || current_params->vlan_id > 0) {
            if (current_params->pcp > 7) {
                oam_pr_debug(current_params->log_file, "[%s] allowed PCP range is 0 - 7, setting to 0.\n", current_params->if_name);
                current_session.pcp = 0;
            } else {
                current_session.pcp = current_params->pcp;
            }
            current_session.vlan_id = current_params->vlan_id;
            current_session.dei = current_params->dei;
            current_session.custom_vlan = true;

            /* Build Ethernet header */
            eth_ptag = libnet_build_802_1q(
                (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
                (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                ETHERTYPE_VLAN,                                         /* Tag protocol identifier */
                current_session.pcp,                                    /* Priority code point */
                current_session.dei,                                    /* Drop eligible indicator(formerly CFI) */
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
    } else
        current_session.is_if_tagged = true;

    /* Initial TX timer configuration */
    tx_sev.sigev_notify = SIGEV_THREAD;                         /* Notify via thread */
    tx_sev.sigev_notify_function = &lbm_timeout_handler;        /* Handler function */
    tx_sev.sigev_notify_attributes = NULL;                      /* Could be pointer to pthread_attr_t structure */
    tx_sev.sigev_value.sival_ptr = &current_session;            /* Pointer passed to handler */

    /* Configure TX interval */
    tx_ts.it_interval.tv_sec = current_session.interval_ms / 1000;
    tx_ts.it_interval.tv_nsec = current_session.interval_ms % 1000 * 1000000;
    tx_ts.it_value.tv_sec = current_session.interval_ms / 1000;
    tx_ts.it_value.tv_nsec = current_session.interval_ms % 1000 * 1000000;

    /* Create TX timer */
    if (timer_create(CLOCK_MONOTONIC, &tx_sev, &(tx_timer.timer_id)) == -1) {
        oam_pr_error(current_params->log_file, "Cannot create timer.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Timer should be created, but we still get a NULL pointer sometimes */
    tx_timer.is_timer_created = true;
    oam_pr_debug(current_params->log_file, "TX timer ID: %p\n", tx_timer.timer_id);

    /* Create a raw socket for incoming frames */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_OAM))) == -1) {
        oam_pr_error(current_params->log_file, "Cannot create socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Store the sockfd so we can close it from the cleanup handler */
    current_session.sockfd = sockfd;

    /* Enable packet auxdata */
    if (setsockopt(sockfd, SOL_PACKET, PACKET_AUXDATA, &flag_enable, sizeof(flag_enable)) < 0) {
        oam_pr_error(current_params->log_file, "Can't enable packet auxdata.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if_index = if_nametoindex(current_params->if_name);
    if (if_index == 0) {
        oam_pr_error(current_params->log_file, "if_nametoindex.\n");
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
        oam_pr_error(current_params->log_file, "Cannot bind socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Session configuration is successful, return a valid session id */
    current_session.is_session_configured = true;

    /* Start timer */
    if (timer_settime(tx_timer.timer_id, 0, &tx_ts, NULL) == -1) {
        oam_pr_error(current_params->log_file, "timer_settime.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    bool got_reply = true;
    bool frame_sent = false;

    oam_pr_debug(current_params->log_file, "LBM session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Processing loop for incoming frames */
    while (true) {

        if (current_session.send_next_frame == true) {

            /* We did not get a reply */
            if (got_reply == false) {

                if (current_session.is_multicast == true) {
                    oam_pr_info(current_params->log_file, "[%s] No replies to multicast LBM, trans_id: %d\n",
                            current_params->if_name, current_session.transaction_id);

                    lbm_multicast_replies = 0;
                } else {
                    
                    if (current_session.is_if_tagged == true)
                        oam_pr_info(current_params->log_file, "[%s] Request timeout for: %02X:%02X:%02X:%02X:%02X:%02X, trans_id: %d.\n",
                                current_params->if_name, dst_hwaddr[0], dst_hwaddr[1], dst_hwaddr[2], dst_hwaddr[3], dst_hwaddr[4],
                                dst_hwaddr[5], current_session.transaction_id);
                    else
                        oam_pr_info(current_params->log_file, "[%s.%u] Request timeout for: %02X:%02X:%02X:%02X:%02X:%02X, trans_id: %d.\n",
                                current_params->if_name, current_session.vlan_id, dst_hwaddr[0], dst_hwaddr[1], dst_hwaddr[2], dst_hwaddr[3],
                                dst_hwaddr[4], dst_hwaddr[5], current_session.transaction_id);
                                current_session.transaction_id);

                    /* Adjust callback related values */
                    lbm_missed_pings++;
                    lbm_replied_pings = 0;
                    is_lbm_session_recovered = false;
                }
            }
            frame_sent = false;
            got_reply = false;

            /* If we reached the missed pings threshold, use callback */
            if (current_params->missed_consecutive_ping_threshold > 0) {
                if (lbm_missed_pings == current_params->missed_consecutive_ping_threshold) {
                    if (current_params->callback != NULL) {
                        callback_status.cb_ret = OAM_LB_CB_MISSED_PING_THRESH;
                        current_params->callback(&callback_status);
                    }

                /* Reset counter */
                lbm_missed_pings = 0;

                /* If it is oneshot operation, close session */
                if (current_params->is_oneshot == true)
                    pthread_exit(NULL);
                }
            }

            if (frame_sent == false) {
            
                /* Bump transaction id */
                current_session.transaction_id++;
            
                /* Update frame and send on wire */
                oam_build_lb_frame(current_session.transaction_id, 0, &lb_frame);
            
                if (current_session.pcp > 0 || current_session.vlan_id) {
                    eth_ptag = libnet_build_802_1q(
                        (uint8_t *)dst_hwaddr,                                  /* Destination MAC */
                        (uint8_t *)src_hwaddr,                                  /* MAC of local interface */
                        ETHERTYPE_VLAN,                                         /* Tag protocol identifier */
                        current_session.pcp,                                    /* Priority code point */
                        current_session.dei,                                    /* Drop eligible indicator(formerly CFI) */
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

                if (eth_ptag == -1) {
                    oam_pr_error(current_params->log_file, "Can't build LBM frame: %s\n", libnet_geterror(l));
                    current_session.send_next_frame = false;
                    continue;
                }

                if (libnet_write(l) == -1) {
                    oam_pr_error(current_params->log_file, "Write error: %s\n", libnet_geterror(l));
                    current_session.send_next_frame = false;
                    continue;
                }

                /* Get aprox timestamp of sent frame */
                clock_gettime(CLOCK_MONOTONIC, &(current_session.time_sent));
                oam_pr_debug(current_params->log_file, "[%s] Sent LBM to: %02X:%02X:%02X:%02X:%02X:%02X, trans_id: %d\n", current_params->if_name,
                    dst_hwaddr[0], dst_hwaddr[1], dst_hwaddr[2], dst_hwaddr[3], dst_hwaddr[4], dst_hwaddr[5], current_session.transaction_id);

                current_session.send_next_frame = false;
                frame_sent = true;
            } // if (frame_sent == false)
        } // if (current_session.send_next_frame == true)

        /* We need another loop, in case session is multicast */
        while (true && (current_session.send_next_frame != true)) {
            
            /* Check for incoming data */
            if (recvmsg_ppoll(sockfd, &recv_hdr, current_session.interval_ms) > 0) {

                /* Get aprox timestamp of received frame */
                clock_gettime(CLOCK_MONOTONIC, &current_session.time_received);

                /* Get ETH header */
                eh = (struct ether_header *)recv_buf;

                /* If frame is not addressed to this interface, drop it */
                if (memcmp(eh->ether_dhost, src_hwaddr, ETH_ALEN) != 0)
                    continue;
    
                /* Is the received frame tagged? */
                if (oam_is_frame_tagged(&recv_hdr, &recv_auxdata) == true) {

                    /*
                    * If frame is tagged, but we didn't add a custom header ourselves, it should be dropped,
                    * as it is intended for a VLAN ETH that has this interface as a primary one.
                     */
                    if (current_session.custom_vlan == false)
                        continue;
                    else
                        /* If we did add a custom tag, check for correct VLAN ID */
                        if ((recv_auxdata.tp_vlan_tci & 0xfff) != current_session.vlan_id)
                            continue;
                }

                /* If frame is not OAM LBR, discard it */
                lbm_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
                if (lbm_frame_p->oam_header.opcode != OAM_OP_LBR)
                    continue;
    
                /* Check MEG level*/
                if (((lbm_frame_p->oam_header.byte1.meg_level >> 5) & 0x7) != current_session.meg_level) {
                    oam_pr_debug(current_params->log_file, "Ignoring LBR with different MEG level: %d != %d\n", ((lbm_frame_p->oam_header.byte1.meg_level >> 5) & 0x7),
                                 current_session.meg_level);
                    continue;
                }

                /* Check transaction ID */
                if (ntohl(lbm_frame_p->transaction_id) != current_session.transaction_id) {
                    oam_pr_debug(current_params->log_file, "Ignoring LBR with different trans_id = %d\n", ntohl(lbm_frame_p->transaction_id));
                    continue;
                }

                /* We are receiving pings, reset missed counter */
                lbm_missed_pings = 0;
                lbm_replied_pings++;

                if (current_session.is_multicast == true)
                    lbm_multicast_replies++;

                /* If we are starting on a tagged interface, don't print the vlan_id (as it should come from the interface name) */
                if (current_session.is_if_tagged == true)
                    oam_pr_info(current_params->log_file, "[%s] Got LBR from: %02X:%02X:%02X:%02X:%02X:%02X, trans_id: %d, time: %.3f ms\n",
                            current_params->if_name, eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3],
                            eh->ether_shost[4],eh->ether_shost[5], ntohl(lbm_frame_p->transaction_id),
                            ((current_session.time_received.tv_sec - current_session.time_sent.tv_sec) * 1000 +
                            (current_session.time_received.tv_nsec - current_session.time_sent.tv_nsec) / 1000000.0));
                else
                    oam_pr_info(current_params->log_file, "[%s.%u] Got LBR from: %02X:%02X:%02X:%02X:%02X:%02X, trans_id: %d, time: %.3f ms\n",
                            current_params->if_name, current_session.vlan_id, eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3],
                            eh->ether_shost[4],eh->ether_shost[5], ntohl(lbm_frame_p->transaction_id),
                            ((current_session.time_received.tv_sec - current_session.time_sent.tv_sec) * 1000 +
                            (current_session.time_received.tv_nsec - current_session.time_sent.tv_nsec) / 1000000.0));

                got_reply = 1;
                frame_sent = 0;

                /* If we missed pings before, we are on a recovery path */
                if (current_params->ping_recovery_threshold > 0) {
                    if (is_lbm_session_recovered == false) {

                        /* We reached recovery threshold, use callback */
                        if (current_params->ping_recovery_threshold == lbm_replied_pings) {
                            is_lbm_session_recovered = true;
                            if (current_params->callback != NULL) {
                                callback_status.cb_ret = OAM_LB_CB_RECOVER_PING_THRESH;
                                current_params->callback(&callback_status);
                            }
                        }
                    }
                }

                /* If session is multicast, check for data again, otherwise break the loop */
                if (current_session.is_multicast == false)
                    break;
            
            } // if (recvmsg_ppoll > 0)
        } // multicast loop
    } // while (true)

    pthread_cleanup_pop(0);

    /* Should never reach this */
    return NULL;
}

/* Entry point of a new OAM LBR session */
void *oam_session_run_lbr(void *args)
{
    struct oam_session_thread *current_thread = (struct oam_session_thread *)args;
    struct oam_lb_session_params *current_params = current_thread->session_params;
    uint8_t src_hwaddr[ETH_ALEN];
    uint8_t dst_hwaddr[ETH_ALEN];
    int flag_enable = 1;
    int if_index;
    struct ether_header *eh;
    struct oam_lb_pdu *lbr_frame_p;
    struct oam_lb_session current_session;
    const uint8_t eth_broadcast[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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

    current_session.meg_level = current_params->meg_level;
    current_session.l = NULL;
    current_session.sockfd = 0;

    /* Install session cleanup handler */
    pthread_cleanup_push(lb_session_cleanup, (void *)&current_session);

    /* Check for CAP_NET_RAW capability */
    caps = cap_get_proc();
    if (caps == NULL) {
        oam_pr_error(current_params->log_file, "cap_get_proc.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_get_flag(caps, CAP_NET_RAW, CAP_EFFECTIVE, &cap_val) == -1) {
        oam_pr_error(current_params->log_file, "cap_get_flag.\n");
        cap_free(caps);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    if (cap_val != CAP_SET) {
        cap_free(caps);
        oam_pr_error(current_params->log_file, "Execution requires CAP_NET_RAW capability.\n");
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
            oam_pr_error(current_params->log_file, "open ns fd.\n");
            current_thread->ret = -1;
            sem_post(&current_thread->sem);
            pthread_exit(NULL);
        }

        if (setns(ns_fd, CLONE_NEWNET) == -1) {
            oam_pr_error(current_params->log_file, "set ns.\n");
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
        oam_pr_error(current_params->log_file, "libnet_init() failed: %s\n", libnet_errbuf);
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get source MAC address */
    if (oam_get_eth_mac(current_params->if_name, src_hwaddr) == -1) {
        oam_pr_error(current_params->log_file, "Error getting MAC address of local interface.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Copy libnet pointer */
    current_session.l = l;

    /* Create a raw socket for incoming frames */
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_OAM))) == -1) {
        oam_pr_error(current_params->log_file, "Cannot create socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Copy socket fd */
    current_session.sockfd = sockfd;

    /* Enable packet auxdata */
    if (setsockopt(sockfd, SOL_PACKET, PACKET_AUXDATA, &flag_enable, sizeof(flag_enable)) < 0) {
        oam_pr_error(current_params->log_file, "Can't enable packet auxdata.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Get interface index */
    if_index = if_nametoindex(current_params->if_name);
    if (if_index == 0) {
        oam_pr_error(current_params->log_file, "if_nametoindex.\n");
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
        oam_pr_error(current_params->log_file, "Cannot bind socket.\n");
        current_thread->ret = -1;
        sem_post(&current_thread->sem);
        pthread_exit(NULL);
    }

    /* Seed random generator used for multicast LBRs */
    srandom((uint64_t)current_params);

    /* Session configuration is successful, return a valid session id */
    current_session.is_session_configured = true;

    oam_pr_debug(current_params->log_file, "LBR session configured successfully.\n");
    sem_post(&current_thread->sem);

    /* Processing loop for incoming packets */
    while (true) {

        /* Wait for data on the socket */
        numbytes = recvmsg(sockfd, &recv_hdr, 0);

        /* We got something, look around */
        if (numbytes > 0) {

            oam_pr_debug(current_params->log_file, "Received frame on LBR session, %ld bytes.\n", numbytes);
            
            /* If frame has a tag, it is not for us */
            if (oam_is_frame_tagged(&recv_hdr, NULL) == true)
                continue;
            
            /* Get ETH header */
            eh = (struct ether_header *)recv_buf;

            /* If frame is not OAM LBM, discard it */
            lbr_frame_p = (struct oam_lb_pdu *)(recv_buf + sizeof(struct ether_header));
            if (lbr_frame_p->oam_header.opcode != OAM_OP_LBM)
                continue;
            
            /* Is frame addressed to this interface? */
            if (memcmp(eh->ether_dhost, src_hwaddr, ETH_ALEN) != 0) {

                /* Is it broadcast or multicast? */
                if((memcmp(eh->ether_dhost, eth_broadcast, ETH_ALEN) == 0) || (eh->ether_dhost[0] == 0x1))
                    current_session.is_frame_multicast = true;
                else
                    /* Otherwise drop it */
                    continue;
            }

            /* Check MEG level*/
            if (((lbr_frame_p->oam_header.byte1.meg_level >> 5) & 0x7) != current_session.meg_level) {
                oam_pr_debug(current_params->log_file, "Ignoring LBM with different MEG level: %d != %d\n", ((lbr_frame_p->oam_header.byte1.meg_level >> 5) & 0x7),
                            current_session.meg_level);
                continue;
            }

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
        
            /* If frame is multicast/broadcast, add delay between 0s - 1s as per standard */
            if (current_session.is_frame_multicast == true) {
                usleep(1000 * (random() % 1000));
                current_session.is_frame_multicast = false;
            }

            /* Send frame on wire */
            if (libnet_write(l) == -1) {
                oam_pr_error(current_params->log_file, "Write error: %s\n", libnet_geterror(l));
                continue;
            }
        }
    } // while (true)

    pthread_cleanup_pop(0);

    /* Should never reach this */
    return NULL;
}

static void lbm_timeout_handler(union sigval sv)
{
    struct oam_lb_session *current_session = sv.sival_ptr;

    current_session->send_next_frame = true;
}

static void lb_session_cleanup(void *args)
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