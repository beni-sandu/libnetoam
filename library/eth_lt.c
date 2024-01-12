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

/* Forward declarations */
void *oam_session_run_ltm(void *args);
void *oam_session_run_ltr(void *args);

/* Per thread variables */
static __thread libnet_t *l;
static __thread char libnet_errbuf[LIBNET_ERRBUF_SIZE];
static __thread struct oam_ltm_pdu ltm_frame;
static __thread libnet_ptag_t eth_ptag = 0;
static __thread cap_t caps;
static __thread cap_flag_value_t cap_val;
static __thread int ns_fd;
static __thread char ns_buf[MAX_PATH] = "/run/netns/";

void *oam_session_run_ltm(void *args)
{
    return NULL;
}

void *oam_session_run_ltr(void *args)
{
    return NULL;
}