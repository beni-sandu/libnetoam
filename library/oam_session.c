/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <pthread.h>

#include "../include/oam_session.h"
#include "../include/eth_lb.h"
#include "../include/eth_lt.h"
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
        case OAM_SESSION_LTM:
            ret = pthread_create(&session_id, NULL, oam_session_run_ltm, (void *)&new_thread);
            break;
        case OAM_SESSION_LTR:
            ret = pthread_create(&session_id, NULL, oam_session_run_ltr, (void *)&new_thread);
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