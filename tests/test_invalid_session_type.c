#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lbm = 0;
    int test_status = 0;

    struct oam_lb_session_params s1_params = {
        .if_name = "veth1",
        .dst_mac = "00:11:22:33:44:55",
        .interval_ms = 5000,
        .missed_consecutive_ping_threshold = 5,
        .ping_recovery_threshold = 5,
        .meg_level = 0,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Test invalid session start */
    s1_lbm = oam_session_start(&s1_params, 9);

    if (s1_lbm == -1)
        printf("PASS: test invalid session start.\n");
    else {
        printf("FAIL: test invalid session start.\n");
        test_status = -1;
    }

    return test_status;
}
