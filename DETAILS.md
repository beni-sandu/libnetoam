

Supported OAM protocols
-----------------------
* ETH-LB (Ethernet loopback function)

Supported parameters for a LBM session (OAM_SESSION_LBM)
--------------------------------------
- if_name - Name of interface to use for the session
- dst_mac - Destination hardware address
- interval_ms - Timeout interval in miliseconds between pings
- missed_consecutive_ping_threshold - Threshold value for missed replies
- ping_recovery_threshold - Threshold value for recovered sessions
- is_oneshot - Flag for oneshot operation mode (session is stopped after missed ping value is reached)
- callback - Callback function that can act on threshold values
- net_ns - Network namespace
- meg_level - Maintenance entity group level (ETH-LB specific)
- vlan_id - Virtual LAN identifier
- pcp - Priority code point (from 802.1q header)
- log_file - Path to a log file that can be used to store log messages
- dei - Drop eligible indicator (from 802.1q header)
- is_multicast - Flag used to configure an ETH-LB multicast session
- enable_console_logs - If enabled, print messages to console too
- log_utc - If enabled, print log messages in UTC timezone

Supported parameters for a LBR session (OAM_SESSION_LBR)
--------------------------------------
- if_name - Name of interface to use for the session
- meg_level - Maintenance entity group level
- log_file - Path to log file used to store log/error info
- net_ns - Network namespace
- enable_console_logs - If enabled, print messages to console too
- log_utc - If enabled, print log messages in UTC timezone

Supported parameters for a LB discovery session (OAM_SESSION_LB_DISCOVER)
--------------------------------------
- if_name - Name of interface to use for the session
- dst_mac_list - List of MAC addresses in string format used to send LBM pings to (**NEEDS TO BE NULL TERMINATED**)
- meg_level - Maintenance entity group level
- interval_ms - Timeout interval in miliseconds between pings (5000ms min)
- callback - Callback function that can be used to retrieve list of live peers
- vlan_id - Virtual LAN identifier
- pcp - Priority code point (from 802.1q header)
- dei - Drop eligible indicator (from 802.1q header)
- log_file - Path to log file used to store log/error info
- net_ns - Network namespace
- enable_console_logs - If enabled, print messages to console too
- log_utc - If enabled, print log messages in UTC timezone

Example of a parameter structure for a LBM session:

```c
struct oam_lb_session_params s1_lbm_params = {
        .if_name = "eth0",
        .dst_mac = "74:78:27:28:bb:cc",
        .interval_ms = 1000,
        .missed_consecutive_ping_threshold = 5,
        .callback = &lbm_callback,
    };
```

Library interfaces
------------------
```c
/*
 * Create a new OAM session.
 *
 * @params:                 pointer to a parameter structure.
 * @session_type:           type of session, currently can be:
 *                              - OAM_SESSION_LBM
 *                              - OAM_SESSION_LBR
 *                              - OAM_SESSION_LB_DISCOVER
 * 
 * Returns a valid session id on successful creation or
 * -1 if an error occured.
 */
oam_session_id oam_session_start(void *params, enum oam_session_type session_type);

/* 
 * Stop a OAM session that has been started.
 * 
 * @session_id: a OAM session id
 */
void oam_session_stop(oam_session_id session_id);

/*
 * Return a string describing library version.
 */
const char *netoam_lib_version(void);
```