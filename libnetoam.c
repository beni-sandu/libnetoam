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

#include <sys/types.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <ctype.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "libnetoam.h"

/* Prototypes */
int hex2bin(char ch);

/* Temporarily used for testing */
void set_promisc(const char *ifname, bool enable, int *sfd)
{
    struct packet_mreq mreq = {0};
    int action;

    if ((*sfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
        perror("unable to open socket");
        return;
    }

    mreq.mr_ifindex = if_nametoindex(ifname);
    mreq.mr_type = PACKET_MR_PROMISC;

    if (mreq.mr_ifindex == 0) {
        perror("unable to get interface index");
        return;
    }

    if (enable)
        action = PACKET_ADD_MEMBERSHIP;
    else
        action = PACKET_DROP_MEMBERSHIP;

    if (setsockopt(*sfd, SOL_PACKET, action, &mreq, sizeof(mreq)) != 0) {
        perror("unable to enter promiscouous mode");
        return;
    }
}

int hex2bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return EXIT_FAILURE;
}

int hwaddr_str2bin(char *mac, uint8_t *addr)
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

int get_eth_mac(char *if_name, uint8_t *mac_addr)
{
    struct ifaddrs *addrs, *ifp;
    struct sockaddr_ll *sa;

    /* Get a list of network interfaces on the system */
    if (getifaddrs(&addrs) == -1) {
        perror("getifaddrs");
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

/* All this code just to find out if an interface is a VLAN, WHY GOD */
bool is_eth_vlan(char *if_name)
{

#define RECV_BUFSIZE (32678)

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
        perror("socket");
        return false;
    }

    /* Send the request to kernel */
    if (sendmsg(sfd, &msg, 0) < 0) {
        perror("sendmsg");
        return false;
    }

    /* Swap header payload */
    uint8_t recv_buf[RECV_BUFSIZE];
    iov->iov_base = &recv_buf;
    iov->iov_len = RECV_BUFSIZE;

    /* Read kernel reply */
    while (1) {
        int len = recvmsg(sfd, &msg, 0);
        if (len < 0) {
            perror("recvmsg");
            return false;
        }

        /* Look through the message */
        for (nh = (struct nlmsghdr *)recv_buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
            int rta_len;
            struct ifinfomsg *msg;
            struct rtattr *rta;

            /* End of multipart message, interface not found */
            if (nh->nlmsg_type == NLMSG_DONE)
                return false;

            /* Error reading message */
            if (nh->nlmsg_type ==  NLMSG_ERROR)
                return false;

            msg = (struct ifinfomsg *)NLMSG_DATA(nh);
            if (msg->ifi_type != ARPHRD_ETHER)
                continue;

            /* We found our interface */
            if ((int)if_nametoindex(if_name) == msg->ifi_index) {

                /* Read message attributes */
                rta = IFLA_RTA(msg);
                rta_len = nh->nlmsg_len - NLMSG_LENGTH(sizeof *msg);

                for (; RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {

                    /* Is it a VLAN interface? */
                    if (rta->rta_type == IFLA_VLAN_PROTOCOL)
                        return true;
                }

                return false;
            }
        }
    }
}

/* Return library version */
const char *netoam_lib_version(void)
{
    return ("libnetoam version "LIBNETOAM_VERSION);
}
