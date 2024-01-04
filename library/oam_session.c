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

#include <pthread.h>

#include "../include/oam_session.h"
#include "../include/eth_lb.h"
#include "../include/libnetoam.h"

/* 
 * Create a new OAM session, returns a session id
 * on successful creation, -1 otherwise
 */
oam_session_id oam_session_start(void *params, enum oam_session_type session_type)
{    
    pthread_t session_id;
    int ret = 0;
    struct oam_session_thread new_thread;

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
            oam_pr_error(NULL, "Invalid OAM session type.\n");
    }

    if (ret) {
        oam_pr_error(NULL, "oam_session_start, err: %d\n", ret);
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
        oam_pr_debug(NULL, "Stopping OAM session: %ld\n", session_id);
        pthread_cancel(session_id);
        pthread_join(session_id, NULL);
    }
}