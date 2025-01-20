#include "oam_test.h"

static int callback_status = OAM_LB_CB_DEFAULT;

/* Prototypes */
void oam_callback(struct cb_status *status);

void oam_callback(struct cb_status *status)
{
    switch (status->cb_ret) {
        case OAM_LB_CB_MISSED_PING_THRESH:
            callback_status = OAM_LB_CB_MISSED_PING_THRESH;
            break;
        case OAM_LB_CB_RECOVER_PING_THRESH:
            callback_status = OAM_LB_CB_RECOVER_PING_THRESH;
            break;
    }
}

int main(void)
{
    oam_session_id s1_lbm = 0, s1_lbr = 0;
    int test_status = 0;
    uint8_t dst_mac[ETH_ALEN];

    struct oam_lb_session_params s1_lbm_params = {
        .if_name = "veth0",
        .interval_ms = 1000,
        .missed_consecutive_ping_threshold = 2,
        .ping_recovery_threshold = 2,
        .meg_level = 0,
        .callback = &oam_callback,
    };

    struct oam_lb_session_params s1_lbr_params = {
        .if_name = "veth1",
        .meg_level = 0,
    };

    /* Get MAC addresses of peers */
    if (oam_get_eth_mac(s1_lbr_params.if_name, dst_mac, NULL) == -1) {
        printf("Failed to get MAC address of %s.\n", s1_lbr_params.if_name);
        return -1;
    }
    oam_hwaddr_bin2str(dst_mac, s1_lbm_params.dst_mac);

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /* Test LBM session start */
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: test LBM session start.\n");
    else {
        printf("FAIL: test LBM session start.\n");
        test_status = -1;
    }

    sleep(4);

    /* Test LBR session start */
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: test LBR session start.\n");
    else {
        printf("FAIL: test LBR session start.\n");
        test_status = -1;
    }

    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: Working LBM/LBR session.\n");
    else {
        printf("FAIL: Working LBM/LBR session.\n");
        test_status = -1;
    }

    if (oam_set_if(s1_lbm_params.if_name, 0) == -1) {
        printf("FAIL: could not set interface %s down.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_MISSED_PING_THRESH)
        printf("PASS: Callback detected missed pings.\n");
    else {
        printf("FAIL: Callback detected missed pings.\n");
        test_status = -1;
    }

    if (oam_set_if(s1_lbm_params.if_name, 1) == -1) {
        printf("FAIL: could not set interface %s up.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: Callback detected recovered session.\n");
    else {
        printf("FAIL: Callback detected recovered session.\n");
        test_status = -1;
    }

    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    return test_status;
}
