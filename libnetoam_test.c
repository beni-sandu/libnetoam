#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libnetoam/libnetoam.h>

void lbm_callback(struct cb_status *status);

// Callback to act on link change
void lbm_callback(struct cb_status *status) {
    
    switch(status->cb_ret) {
        case OAM_LB_CB_MISSED_PING_THRESH:
            oam_pr_info(status->session_params->log_file, "[%s] Consecutive missed ping threshold reached.\n",
                            status->session_params->if_name);
            break;
        
        case OAM_LB_CB_RECOVER_PING_THRESH:
            oam_pr_info(status->session_params->log_file, "[%s] Recovery threshold reached.\n", status->session_params->if_name);
            break;
    }
}

int main(void)
{
    oam_session_id s1_lbm = 0;

    struct oam_lb_session_params s1_params = {
        .if_name = "enp2s0",
        .dst_mac = "00:e0:4c:66:70:7f",
        .interval_ms = 5000,
        .missed_consecutive_ping_threshold = 5,
        .ping_recovery_threshold = 5,
        .callback = &lbm_callback,
        .meg_level = 0,
    };
    
    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    s1_lbm = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("[%s] LBM session started successfully.\n", s1_params.if_name);
    else
        printf("[%s] Failed to start LBM session.\n", s1_params.if_name);
    
    sleep(3600);
    oam_session_stop(s1_lbm);

    return EXIT_SUCCESS;
}