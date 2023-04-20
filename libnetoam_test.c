#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libnetoam/libnetoam.h>

void lbm_callback(struct cb_status *status);

void lbm_callback(struct cb_status *status) {
    
    switch(status->cb_ret) {
        case OAM_CB_MISSED_PING_THRESH:
            printf("[%s] Consecutive missed ping threshold reached.\n", status->session_params->if_name);
            break;
    }
}

int main(void)
{
    oam_session_id s1 = 0;

    struct oam_session_params s1_params = {
        .if_name = "enp2s0",
        .dst_mac = "74:78:27:fc:b2:93",
        .interval_ms = 1000,
        .missed_consecutive_ping_threshold = 5,
        .callback = &lbm_callback,
    };
    
    printf("Running with: %s\n", netoam_lib_version());
    pr_debug("NOTE: You are running a debug build.\n");

    s1 = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1 < 0)
        fprintf(stderr, "OAM LBM session start failed.\n");
    else
        printf("Session started, id: %ld\n", s1);

    sleep(3000);

    oam_session_stop(s1);

    return EXIT_SUCCESS;
}