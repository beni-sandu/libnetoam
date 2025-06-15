#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lbm = 0, s1_lbr = 0, s2_lbr = 0, s3_lbr = 0;
    int test_status = 0;

    struct oam_lb_session_params s1_lbm_params = {
        .if_name = "lbm-peer",
        .interval_ms = 5000,
        .meg_level = 0,
        .is_multicast = true,
        .enable_console_logs = true,
    };

    struct oam_lb_session_params s1_lbr_params = {
        .if_name = "lbr1-peer",
        .meg_level = 0,
    };

    struct oam_lb_session_params s2_lbr_params = {
        .if_name = "lbr2-peer",
        .meg_level = 0,
    };

    struct oam_lb_session_params s3_lbr_params = {
        .if_name = "lbr3-peer",
        .meg_level = 1,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Start LBR sessions */
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("LBR session started.\n");
    else {
        printf("Failed to start LBR session.\n");
        test_status = -1;
    }

    s2_lbr = oam_session_start(&s2_lbr_params, OAM_SESSION_LBR);
    if (s2_lbr > 0)
        printf("LBR session started.\n");
    else {
        printf("Failed to start LBR session.\n");
        test_status = -1;
    }

    s3_lbr = oam_session_start(&s3_lbr_params, OAM_SESSION_LBR);
    if (s3_lbr > 0)
        printf("LBR session started.\n");
    else {
        printf("Failed to start LBR session.\n");
        test_status = -1;
    }
    sleep(2);

    /* Start LBM session */
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);
    if (s1_lbm > 0)
        printf("Multicast LBM session started.\n");
    else {
        printf("Failed to start multicast LBM session.\n");
        test_status = -1;
    }

    /* Wait for a few multicast replies */
    sleep(10);

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);
    oam_session_stop(s2_lbr);
    oam_session_stop(s3_lbr);

    return test_status;
}
