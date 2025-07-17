#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lb_d = 0;
    int test_status = 0;

    const char *mac_list_0[] = {
        "aa:bb:cc:dd:ee:ff",
        "11:22:33:44:55:66",
        "aa:bb:cc:11:22:33",
        NULL
    };

    const char *mac_list_1[] = {
        "12:34:56:78:90:12",
        "a1:a2:a3:a4:a5:a6",
        "1a:2b:3c:41:52:63",
        NULL
    };

    const char *mac_list_2[] = {
        "10:20:30:40:50:60",
        "a0:a1:a2:a3:a4:a5",
        "5a:6b:7c:81:92:03",
        NULL
    };

    struct oam_lb_session_params s1_lb_d_params = {
        .if_name = "veth0",
        .interval_ms = 5000,
        .meg_level = 0,
        .enable_console_logs = true,
        .dst_mac_list = mac_list_0,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Start LB_DISCOVER session */
    s1_lb_d = oam_session_start(&s1_lb_d_params, OAM_SESSION_LB_DISCOVER);
    if (s1_lb_d > 0)
        printf("[PASS] LB_DISCOVER session start multiple MAC lists.\n");
    else {
        printf("[FAIL] LB_DISCOVER session start multiple MAC lists.\n");
        test_status = -1;
    }

    /* Request update of list */
    sleep(10);
    s1_lb_d_params.dst_mac_list = mac_list_1;
    s1_lb_d_params.update_mac_list = true;

    /* Request update of list */
    sleep(7);
    s1_lb_d_params.dst_mac_list = mac_list_2;
    s1_lb_d_params.update_mac_list = true;

    /* Stop sessions */
    sleep(10);
    printf("[PASS] LB_DISCOVER session update MAC lists.\n");
    oam_session_stop(s1_lb_d);

    return test_status;
}
