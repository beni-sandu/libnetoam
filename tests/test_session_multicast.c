#include "oam_test.h"

#define LIVE_PEER_LIST (12)
static uint8_t live_peers[LIVE_PEER_LIST][ETH_ALEN] = { {0} };

/* Prototypes */
void oam_callback(struct cb_status *status);

void oam_callback(struct cb_status *status)
{
    switch (status->cb_ret) {
        case OAM_LB_CB_LIST_LIVE_MACS: {

            /* Print list of current live peers */
            printf("List of live peers:\n");
            for (size_t i = 0; i < LIVE_PEER_LIST && live_peers[i][0]; ++i)
                printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                        live_peers[i][0], live_peers[i][1], live_peers[i][2],
                        live_peers[i][3], live_peers[i][4], live_peers[i][5]);

            /* Clear list for next update */
            memset(live_peers, 0, sizeof(live_peers));
            break;
        }
    }
}

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
        .callback = &oam_callback,
        .client_data = live_peers,
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
