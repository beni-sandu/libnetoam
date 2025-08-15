Change log
==========

All relevant changes are documented in this file.

v0.1.2 released TBD
-------------
* rework transmit interval implementation
* implement a way to retrieve list of live peers for multicast and LB_DISCOVER
* implement a new LB_DISCOVER session type
* fix multicast address range
* remove libnet dependency
* add an ETH-LB multicast testcase
* update deprecated random()/usleep() interfaces
* improve memory leak testcase

v0.1.1 released 22-Jan-2025
-------------
* ETH-LB - use monotonic clocks instead of realtime
* ETH-LB - add a minimum length Sender ID TLV
* ETH-LB - use BP filter instead of ETH_P_ALL sockets
* add an initial testing framework
* improve error reporting and handling
* add **`enable_console_logs`** flag to explicitly output log messages to console
* add **`log_utc`** flag to log messages in UTC timezone
* other general fixes and cleanup

v0.1 released 15-Aug-2023
------------------
First release of the library.
