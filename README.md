Operation, administration and maintenance (OAM) networking protocols
====================================================================

libnetoam is a minimalistic library implementation of the OAM protocols, based on:

- [IEEE 802.1ag - Connectivity Fault Management (CFM)](https://www.ieee802.org/1/pages/802.1ag.html)
- [ITU-T G.8013/Y.1731 - Operation, administration and maintenance (OAM)](https://www.itu.int/rec/T-REC-Y.1731/en)

These specify mechanisms required to operate and maintain the network and service aspects of the ETH layer.

List of implemented OAM protocols
---------------------------------
* ETH-LB (ping at MAC level)

Building and installing
-----------------------
Install needed dependencies first (e.g. on Debian derived systems):

```sh
$ sudo apt install libcap-dev
```

Build and install libnetoam:

```sh
$ git clone https://github.com/beni-sandu/libnetoam.git
$ cd libnetoam
$ make
$ sudo make install
$ sudo ldconfig
```
After the library is installed, you can link it with your program using -lnetoam.

Using the library
-----------------
libnetoam is installed as a shared library and a set of headers. The main header to use in your program is:

```c
#include <libnetoam/libnetoam.h>
```

Below is a code example of a typical workflow:

```c
// Callback to act on link change
void lbm_callback(struct cb_status *status) {
    
    switch(status->cb_ret) {
        case OAM_LB_CB_MISSED_PING_THRESH:
            printf("[%s] Consecutive missed ping threshold reached.\n",
                            status->session_params->if_name);
            break;
        
        case OAM_LB_CB_RECOVER_PING_THRESH:
            printf("[%s] Recovery threshold reached.\n", status->session_params->if_name);
            break;
    }
}

// Fill in needed parameters for a LBM session
oam_session_id s1_lbm = 0, s1_lbr = 0;

struct oam_lb_session_params s1_lbm_params = {
        .if_name = "eth0",
        .dst_mac = "74:78:27:28:bb:cc",
        .interval_ms = 5000,
        .missed_consecutive_ping_threshold = 5,
        .callback = &lbm_callback,
        .meg_level = 0,
};

struct oam_lb_session_params s1_lbr_params = {
        .if_name = "eth0",
};

// Start a LBM session:
s1_lbm = oam_session_start(&s1_lbm_params, OAM_SESSION_LBM);

// ..or LBR session:
s1_lbr = oam_session_start(&s1_lbr_params, OAM_SESSION_LBR);

// Error checking
if (s1_lbm > 0)
    printf("LBM session started successfully: [%s]\n", s1_lbm_params.if_name);
else
    printf("Error starting LBM session: [%s]\n", s1_lbm_params.if_name);

if (s1_lbr > 0)
    printf("LBR session started successfully: [%s]\n", s1_lbr_params.if_name);
else
    printf("Error starting LBR session: [%s]\n", s1_lbr_params.if_name);

// Do your work here...

// Stop the session
oam_session_stop(s1_lbm);
oam_session_stop(s1_lbr);
```

More details about the available interfaces and parameters can be found here:
- [DETAILS.md](DETAILS.md)
