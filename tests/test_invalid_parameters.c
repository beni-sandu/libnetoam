#include "oam_test.h"

int main(void)
{
    oam_session_id s1_lbm = 0;
    int test_status = 0;

    struct oam_lb_session_params s1_params = {
        .if_name = "dummy",
        .dst_mac = "00:11:22:33:44:55",
        .interval_ms = 5000,
        .missed_consecutive_ping_threshold = 5,
        .ping_recovery_threshold = 5,
        .meg_level = 0,
        .enable_console_logs = true,
    };

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Test invalid interface */
    s1_lbm = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1_lbm == -1)
        printf("PASS: test invalid interface name.\n");
    else {
        printf("FAIL: test invalid interface name.\n");
        test_status = -1;
    }
    oam_session_stop(s1_lbm);
    snprintf(s1_params.if_name, sizeof("veth0"), "veth0");
    sleep(1);

    /* Test invalid network namespace */
    snprintf(s1_params.net_ns, sizeof("no_net_ns"), "no_net_ns");
    s1_lbm = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1_lbm == -1)
        printf("PASS: test invalid network namespace.\n");
    else {
        printf("FAIL: test invalid network namespace.\n");
        test_status = -1;
    }
    oam_session_stop(s1_lbm);
    memset(&s1_params.net_ns, 0, sizeof(s1_params.net_ns));
    sleep(1);

    /* Test invalid destination MAC */
    snprintf(s1_params.dst_mac, sizeof("aa:bb:gg"), "aa:bb:gg");
    s1_lbm = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1_lbm == -1)
        printf("PASS: test invalid destination MAC address.\n");
    else {
        printf("FAIL: test invalid destination MAC address.\n");
        test_status = -1;
    }
    oam_session_stop(s1_lbm);

    return test_status;
}
