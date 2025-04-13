/*
 * The following configurations are tested:
 *
 * LBM on main interface           <-----------> LBR on main interface
 * LBM on main interface           <-----------> LBR on VLAN interface
 * LBM on main interface + VLAN ID <-----------> LBR on main interface
 * LBM on main interface + VLAN ID <-----------> LBR on VLAN interface
 * LBM on VLAN interface           <-----------> LBR on main interface
 * LBM on VLAN interface           <-----------> LBR on VLAN interface
 */

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

    /* Start LBM on main interface */
    snprintf(s1_lbm_params.if_name, sizeof("veth0"), "veth0");
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on main interface: %s.\n", s1_lbm_params.if_name);
    else {
        printf("FAIL: start LBM on main interface: %s.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on main interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1"), "veth1");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBM on main interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBM on main interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: LBM on main interface           <-----------> LBR on main interface.\n");
    else {
        printf("FAIL: LBM on main interface           <-----------> LBR on main interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    /* Start LBM on main interface */
    snprintf(s1_lbm_params.if_name, sizeof("veth0"), "veth0");
    callback_status = OAM_LB_CB_DEFAULT;
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on main interface: %s.\n", s1_lbm_params.if_name);
    else {
        printf("FAIL: start LBM on main interface: %s.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on VLAN interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1.295"), "veth1.295");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_MISSED_PING_THRESH)
        printf("PASS: LBM on main interface           <-----------> LBR on VLAN interface.\n");
    else {
        printf("FAIL: LBM on main interface           <-----------> LBR on VLAN interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    /* Start LBM on main interface + VLAN ID */
    callback_status = OAM_LB_CB_DEFAULT;
    snprintf(s1_lbm_params.if_name, sizeof("veth0"), "veth0");
    s1_lbm_params.vlan_id = 295;
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on main interface + VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
    else {
        printf("FAIL: start LBM on main interface + VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on VLAN interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1.295"), "veth1.295");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: LBM on main interface + VLAN ID <-----------> LBR on VLAN interface.\n");
    else {
        printf("FAIL: LBM on main interface + VLAN ID <-----------> LBR on VLAN interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    /* Start LBM on main interface + VLAN ID */
    callback_status = OAM_LB_CB_DEFAULT;
    snprintf(s1_lbm_params.if_name, sizeof("veth0"), "veth0");
    s1_lbm_params.vlan_id = 295;
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on main interface + VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
    else {
        printf("FAIL: start LBM on main interface + VLAN ID: %s.%d.\n", s1_lbm_params.if_name, s1_lbm_params.vlan_id);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on main interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1"), "veth1");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR on main interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR on main interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_MISSED_PING_THRESH)
        printf("PASS: LBM on main interface + VLAN ID <-----------> LBR on main interface.\n");
    else {
        printf("FAIL: LBM on main interface + VLAN ID <-----------> LBR on main interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    /* Start LBM on VLAN interface */
    callback_status = OAM_LB_CB_DEFAULT;
    snprintf(s1_lbm_params.if_name, sizeof("veth0.295"), "veth0.295");
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on VLAN interface: %s.\n", s1_lbm_params.if_name);
    else {
        printf("FAIL: start LBM on VLAN interface: %s.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on main interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1"), "veth1");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR on main interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR on main interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_MISSED_PING_THRESH)
        printf("PASS: LBM on VLAN interface           <-----------> LBR on main interface.\n");
    else {
        printf("FAIL: LBM on VLAN interface           <-----------> LBR on main interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    /* Start LBM on VLAN interface */
    snprintf(s1_lbm_params.if_name, sizeof("veth0.295"), "veth0.295");
    callback_status = OAM_LB_CB_DEFAULT;
    s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

    if (s1_lbm > 0)
        printf("PASS: start LBM on VLAN interface: %s.\n", s1_lbm_params.if_name);
    else {
        printf("FAIL: start LBM on VLAN interface: %s.\n", s1_lbm_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Start LBR on VLAN interface */
    snprintf(s1_lbr_params.if_name, sizeof("veth1.295"), "veth1.295");
    s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);
    if (s1_lbr > 0)
        printf("PASS: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
    else {
        printf("FAIL: start LBR on VLAN interface: %s.\n", s1_lbr_params.if_name);
        test_status = -1;
    }

    /* Wait for status update */
    sleep(4);

    /* Check status */
    if (callback_status == OAM_LB_CB_RECOVER_PING_THRESH)
        printf("PASS: LBM on VLAN interface           <-----------> LBR on VLAN interface.\n");
    else {
        printf("FAIL: LBM on VLAN interface           <-----------> LBR on VLAN interface.\n");
        test_status = -1;
    }

    /* Stop sessions */
    oam_session_stop(s1_lbm);
    oam_session_stop(s1_lbr);

    return test_status;
}
