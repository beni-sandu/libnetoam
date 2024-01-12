/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <ctype.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <time.h>
#include <stdbool.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdarg.h>

#include "../include/libnetoam.h"

#define VLAN_VALID(hdr, hv)   ((hv)->tp_vlan_tci != 0 || ((hdr)->tp_status & TP_STATUS_VLAN_VALID))

/* Prototypes */
static int hex2bin(char ch);

static int hex2bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return EXIT_FAILURE;
}

int oam_hwaddr_str2bin(char *mac, uint8_t *addr)
{
	int i;

	for (i = 0; i < ETH_ALEN; i++) {
		int a, b;

		a = hex2bin(*mac++);
		if (a < 0)
			return EXIT_FAILURE;
		b = hex2bin(*mac++);
		if (b < 0)
			return EXIT_FAILURE;
		*addr++ = (a << 4) | b;
		if (i < ETH_ALEN - 1 && *mac++ != ':')
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int oam_get_eth_mac(char *if_name, uint8_t *mac_addr)
{
    struct ifaddrs *addrs, *ifp;
    struct sockaddr_ll *sa;

    /* Get a list of network interfaces on the system */
    if (getifaddrs(&addrs) == -1) {
        oam_pr_error(NULL, "getifaddrs.\n");
        return EXIT_FAILURE;
    }

    /* Walk through the list and get the MAC of our interface */
    ifp = addrs;

    while (ifp != NULL) {
        if (strcmp(if_name, ifp->ifa_name) == 0) {

            /* We found our interface, copy the HW address */
            if (ifp->ifa_addr->sa_family == AF_PACKET) {
                sa = (struct sockaddr_ll *)ifp->ifa_addr;
                for (int i = 0; i < ETH_ALEN; i++)
                    mac_addr[i] = sa->sll_addr[i];
            }

            freeifaddrs(addrs);
            return EXIT_SUCCESS;
        }
        ifp = ifp->ifa_next;
    }

    /* The provided interface is not present on the machine */
    freeifaddrs(addrs);
    return EXIT_FAILURE;
}

/* Returns 0 if interface is VLAN, 1 if not and -1 on error */
int oam_is_eth_vlan(char *if_name)
{
#define RECV_BUFSIZE (4096)

    /* Request message */
    struct req_msq {
        struct nlmsghdr header;
        struct ifinfomsg msg;
    } req;

    /* Fill in request that we send to the kernel */
    size_t seq_num = 0;
    struct nlmsghdr *nh;
    struct sockaddr_nl sa = {0};
    struct iovec iov[1] = { {&req, sizeof(req)} };
    struct msghdr msg = {
        .msg_name = &sa,
        .msg_namelen = sizeof(sa),
        .msg_iov = iov,
        .msg_iovlen = 1,
    };
    req.header.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    req.header.nlmsg_type = RTM_GETLINK;
    req.header.nlmsg_seq = ++seq_num;
    sa.nl_family = AF_NETLINK;
    
    /* Create a netlink route socket */
    int sfd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sfd < 0) {
        oam_pr_error(NULL, "Cannot create NL socket.\n");
        return -1;
    }

    /* Send the request to kernel */
    if (sendmsg(sfd, &msg, 0) < 0) {
        oam_pr_error(NULL, "Error sending NL request to kernel.\n");
        return -1;
    }

    /* Swap header payload */
    uint8_t recv_buf[RECV_BUFSIZE];
    iov->iov_base = &recv_buf;
    iov->iov_len = RECV_BUFSIZE;

    /* Read kernel reply */
    while (1) {
        int len = recvmsg(sfd, &msg, 0);
        if (len < 0) {
            oam_pr_error(NULL, "Error reading NL reply from kernel.\n");
            return -1;
        }

        /* Look through the message */
        for (nh = (struct nlmsghdr *)recv_buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
            int rta_len;
            struct ifinfomsg *ifi;
            struct rtattr *rta;

            /* End of multipart message, interface not found */
            if (nh->nlmsg_type == NLMSG_DONE) {
                oam_pr_error(NULL, "Interface not found for NL reply.\n");
                return -1;
            }

            /* Error reading message */
            if (nh->nlmsg_type ==  NLMSG_ERROR) {
                oam_pr_error(NULL, "Error reading NL message from kernel.\n");
                return -1;
            }

            /* Skip non ETH interfaces */
            ifi = (struct ifinfomsg *)NLMSG_DATA(nh);
            if (ifi->ifi_type != ARPHRD_ETHER)
                continue;

            /* We found our interface */
            if ((int)if_nametoindex(if_name) == ifi->ifi_index) {

                /* Read message attributes */
                rta = IFLA_RTA(ifi);
                rta_len = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));

                for (; RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {

                    /* Get LINKINFO */
                    if (rta->rta_type == IFLA_LINKINFO) {

                        struct rtattr *rtk = RTA_DATA(rta);
                        int rtk_len = RTA_PAYLOAD(rta);

                        /* Follow first chain */
                        for (; RTA_OK(rtk, rtk_len); rtk = RTA_NEXT(rtk, rtk_len)) {
                            
                            /* Get INFO_KIND */
                            if (rtk->rta_type == IFLA_INFO_KIND) {
                                
                                /* Is it a VLAN? */
                                if (!strcmp(((char *)RTA_DATA(rtk)), "vlan"))
                                    return 0;
                            }
                        }
                        
                    }
                }

                return 1;
            }
        }
    }
}

/* Check if ETH frame has a 802.1q header. If it does, copy the auxdata in buffer provided by second parameter */
bool oam_is_frame_tagged(struct msghdr *recv_msg, struct tpacket_auxdata *aux_buf)
{
    for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(recv_msg); cmsg != NULL; cmsg = CMSG_NXTHDR(recv_msg, cmsg)) {
        if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct tpacket_auxdata)) ||
            cmsg->cmsg_level != SOL_PACKET ||
            cmsg->cmsg_type != PACKET_AUXDATA)
				continue;

            struct tpacket_auxdata *auxp = (struct tpacket_auxdata *)CMSG_DATA(cmsg);

            if (VLAN_VALID(auxp, auxp)) {
                if (aux_buf != NULL)
                    memcpy(aux_buf, auxp, sizeof(struct tpacket_auxdata));
                return true;
                break;
            }

    }
    return false;
}

void oam_pr_log(char *log_file, const char *format, ...)
{
    va_list arg;
    time_t now;
    struct tm *local = NULL;
    char timestamp[100];
    FILE *file = NULL;

    if (log_file == NULL)
        return;

    if (strlen(log_file) == 0)
        return;
    else {
        file = fopen(log_file, "a");

        if (file == NULL) {
            perror("fopen");
            return;
        }
    }

    va_start(arg, format);
    now = time(NULL);
    local = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%d-%b-%Y %H:%M:%S", local);
    fprintf(file, "[%s] ", timestamp);
    vfprintf(file, format, arg);
    va_end(arg);
    fclose(file);
}

/* Return library version */
const char *netoam_lib_version(void)
{
    return ("libnetoam version "LIBNETOAM_VERSION);
}
