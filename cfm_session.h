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

#ifndef _CFM_SESSION_H
#define _CFM_SESSION_H

#include <semaphore.h>
#include <stdbool.h>

/* Add a typedef for a CFM session id */
typedef long int cfm_session_id;

/* Types of CFM sessions, we only implement ETH-LB for the moment */
enum cfm_session_type {
    CFM_SESSION_LBM = 0,
    CFM_SESSION_LBR = 1,
};

#define IF_NAME_SIZE 32
#define NET_NS_SIZE 32
#define MAX_PATH 512

struct cfm_thread {
    sem_t sem;
    struct cfm_session_params *session_params;
    int ret;
};

struct cb_status {
    int cb_ret;                                                 /* Callback return value */
    struct cfm_session_params *session_params;                  /* Pointer to current session parameters */
};

struct cfm_session_params {
    char if_name[IF_NAME_SIZE];                                 /* Network interface name */
    char *dst_mac;                                              /* Destination MAC address */
};

#endif //_CFM_SESSION_H