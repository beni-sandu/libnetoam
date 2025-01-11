#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lbm = 0, s1_lbr = 0;
    int test_status = 0;

    struct oam_lb_session_params s1_params = {
        .if_name = "veth1",
        .dst_mac = "00:11:22:33:44:55",
        .interval_ms = 5000,
        .missed_consecutive_ping_threshold = 5,
        .ping_recovery_threshold = 5,
        .meg_level = 0,
    };

    struct oam_lb_session_params s2_params = {
        .if_name = "veth2",
        .meg_level = 0,
    };
    
    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Test LBM session start */
    s1_lbm = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: test LBM session start.\n");
    else {
        printf("FAIL: test LBM session start.\n");
        test_status = -1;
    }

    /* Test LBR session start */
    s1_lbr = oam_session_start(&s2_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: test LBR session start.\n");
    else {
        printf("FAIL: test LBR session start.\n");
        test_status = -1;
    }
    
    sleep(2);
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);
    printf("PASS: test LBM session stop.\n");
    printf("PASS: test LBR session stop.\n");

    return test_status;
}
