/*
 * Copyright (C) 2023 Beniamin Sandu <beniaminsandu@gmail.com>
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

#ifndef _OAM_SESSION_H
#define _OAM_SESSION_H

#include <semaphore.h>

/* Add a typedef for a OAM session id */
typedef long int oam_session_id;

/* Types of OAM sessions, we only implement ETH-LB for the moment */
enum oam_session_type {
    OAM_SESSION_LBM = 0,
    OAM_SESSION_LBR = 1,
};

struct oam_session_thread {
    sem_t sem;
    void *session_params;
    int ret;
};

struct cb_status {
    int cb_ret;                                                 /* Callback return value */
    struct oam_lb_session_params *session_params;                  /* Pointer to current session parameters */
};

enum oam_cb_ret {
    OAM_LB_CB_DEFAULT                  = 0,
    OAM_LB_CB_MISSED_PING_THRESH       = 1,
    OAM_LB_CB_RECOVER_PING_THRESH      = 2,
};

#endif //_OAM_SESSION_H