#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lb_d = 0;
    int test_status = 0;

    const char *mac_list[] = {
        "aa:bb:cc:dd:ee:ff",
        "12:34:555",
        "11:22:33:44:55",
        NULL
    };

    struct oam_lb_session_params s1_lb_d_params = {
        .if_name = "veth0",
        .interval_ms = 5000,
        .meg_level = 0,
        .enable_console_logs = true,
        .dst_mac_list = mac_list,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Start LB_DISCOVER session */
    s1_lb_d = oam_session_start(&s1_lb_d_params, OAM_SESSION_LB_DISCOVER);
    if (s1_lb_d == -1)
        printf("[PASS] LB_DISCOVER bad MAC.\n");
    else {
        printf("[FAIL] LB_DISCOVER bad MAC.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lb_d);

    return test_status;
}
