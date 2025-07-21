/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
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
    OAM_SESSION_LB_DISCOVER = 2,
};

struct oam_session_thread {
    sem_t sem;
    void *session_params;
    int ret;
};

struct cb_status {
    int cb_ret;                                                 /* Callback return value */
    struct oam_lb_session_params *session_params;               /* Pointer to current session parameters */
};

enum oam_cb_ret {
    OAM_LB_CB_DEFAULT                  = 0,
    OAM_LB_CB_MISSED_PING_THRESH       = 1,
    OAM_LB_CB_RECOVER_PING_THRESH      = 2,
    OAM_LB_CB_LIST_LIVE_MACS           = 3,
};

#endif //_OAM_SESSION_H