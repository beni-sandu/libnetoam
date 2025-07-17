#include "oam_test.h"

int main(int argc, char **argv)
{
    oam_session_id s1_lb_d = 0, s1_lbr = 0, s2_lbr = 0, s3_lbr = 0;
    int test_status = 0;
    const char * const *mac_list = (const char * const *)(argv + 1);

    struct oam_lb_session_params s1_lb_d_params = {
        .if_name = "lbm-peer",
        .interval_ms = 5000,
        .meg_level = 0,
        .enable_console_logs = true,
        .dst_mac_list = mac_list,
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
        .meg_level = 0,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");
    oam_pr_info(NULL, "testcase argc = %d.\n", argc);

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

    /* Start LB_DISCOVER session */
    s1_lb_d = oam_session_start(&s1_lb_d_params, OAM_SESSION_LB_DISCOVER);
    if (s1_lb_d > 0)
        printf("LB_DISCOVER session started.\n");
    else {
        printf("Failed to start LB_DISCOVER session.\n");
        test_status = -1;
    }

    /* Wait for replies */
    sleep(10);

    /* Stop sessions */
    oam_session_stop(s1_lb_d);
    oam_session_stop(s1_lbr);
    oam_session_stop(s2_lbr);
    oam_session_stop(s3_lbr);

    return test_status;
}
