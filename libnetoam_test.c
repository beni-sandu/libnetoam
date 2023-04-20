#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libnetoam/libnetoam.h>

int main(void) {

    oam_session_id s1 = 0;

    struct oam_session_params s1_params = {
        .if_name = "enx747827fcb293",
        .dst_mac = "18:31:bf:29:2d:5a",
        .interval_ms = 1000,
    };
    
    printf("Running with: %s\n", netoam_lib_version());
    pr_debug("NOTE: You are running a debug build.\n");

    s1 = oam_session_start(&s1_params, OAM_SESSION_LBM);

    if (s1 < 0)
        fprintf(stderr, "OAM LBM session start failed.\n");
    else
        printf("Session started, id: %ld\n", s1);

    sleep(30);

    oam_session_stop(s1);

    return EXIT_SUCCESS;
}