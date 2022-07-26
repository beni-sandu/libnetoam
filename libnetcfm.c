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

#include "libnetcfm.h"

int hex2bin(char ch) {
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return EXIT_FAILURE;
}

int hwaddr_str2bin(char *mac, uint8_t *addr) {
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

int get_eth_mac(char *if_name, uint8_t *mac_addr) {
    
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

/* Return library version */
const char *netcfm_lib_version(void) {

    return ("libnetcfm version "LIBNETCFM_VERSION);
}
