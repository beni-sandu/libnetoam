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
    oam_session_id s2_lbm = 0, s2_lbr = 0;
    int test_status = 0;
    uint8_t s1_dst_mac[ETH_ALEN];
    uint8_t s2_dst_mac[ETH_ALEN];

    struct oam_lb_session_params s1_lbm_params = {
        .if_name = "veth0",
        .interval_ms = 1000,
        .missed_consecutive_ping_threshold = 2,
        .ping_recovery_threshold = 2,
        .meg_level = 0,
        .vlan_id = 295,
        .callback = &oam_callback,
        .enable_console_logs = true,
    };

    struct oam_lb_session_params s1_lbr_params = {
        .if_name = "veth1.295",
        .meg_level = 0,
    };

    struct oam_lb_session_params s2_lbm_params = {
        .if_name = "veth2.295",
        .interval_ms = 1000,
        .missed_consecutive_ping_threshold = 2,
        .ping_recovery_threshold = 2,
        .meg_level = 0,
        .vlan_id = 99,
        .callback = &oam_callback,
        .enable_console_logs = true,
    };

    struct oam_lb_session_params s2_lbr_params = {
        .if_name = "veth3.295",
        .meg_level = 0,
    };

    /* Get MAC addresses of peers */
    if (oam_get_eth_mac(s1_lbr_params.if_name, s1_dst_mac, NULL) == -1) {
        printf("Failed to get MAC address of %s.\n", s1_lbr_params.if_name);
        return -1;
    }
    oam_hwaddr_bin2str(s1_dst_mac, s1_lbm_params.dst_mac);

    if (oam_get_eth_mac(s2_lbr_params.if_name, s2_dst_mac, NULL) == -1) {
        printf("Failed to get MAC address of %s.\n", s2_lbr_params.if_name);
        return -1;
    }
    oam_hwaddr_bin2str(s2_dst_mac, s2_lbm_params.dst_mac);

    printf("Running with: %s\n", netoam_lib_version());
    oam_pr_debug(NULL, "NOTE: You are running a debug build.\n");

    /*
     * Start the first LBM session.
     * We have a VLAN interface for this ID, but we need to support providing it as a parameter too.
     * In this case, the VLAN header is built within the library.
     */
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM session with provided VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
    else {
        printf("FAIL: start LBM session with provided VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
        test_status = -1;
    }

    /* Allow for callback to update status */
    sleep(4);

   /* Start the first LBR session */
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR session on VLAN interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR session on VLAN interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Allow for callback to update status */
    sleep(4);

    /* We should be getting a working session now (recovered status) */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: working session with provided VLAN ID.\n");
    else {
        printf("FAIL: working session with provided VLAN ID.\n");
        test_status = -1;
    }

    /*
     * Start the second LBM session on the tagged interface.
     * In this case, the library must ignore the provided VLAN ID, and let the network stack
     * handle the tagging.
     */
    s2_lbm = oam_session_start(&s2_lbm_params, OAM_SESSION_LBM);

    if (s2_lbm > 0)
        printf("PASS: start LBM session on tagged interface: %s.\n", s2_lbm_params.if_name);
    else {
        printf("FAIL: start LBM session on tagged interface: %s.\n", s2_lbm_params.if_name);
        test_status = -1;
    }

    /* Allow for callback to update status */
    sleep(4);

    /* Start the second LBR session */
    s2_lbr = oam_session_start(&s2_lbr_params, OAM_SESSION_LBR);
    if (s2_lbr == -1) {
        printf("Failed to start LBR session for %s.\n", s2_lbr_params.if_name);
        test_status = -1;
    }

    /* Allow for callback to update status */
    sleep(4);

    /* We should be getting a working session now (recovered status) */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: working session on tagged interface.\n");
    else {
        printf("FAIL: working session on tagged interface.\n");
        test_status = -1;
    }

    sleep(60);

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);
    oam_session_stop(s2_lbm);
    oam_session_stop(s2_lbr);

    return test_status;
}
