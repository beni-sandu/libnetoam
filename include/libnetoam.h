/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _LIBNETOAM_H
#define _LIBNETOAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <linux/if_packet.h>

#include "oam_session.h"
#include "eth_lb.h"

/* Library version */
#define LIBNETOAM_VERSION "0.1"

/* Print macros */
#ifdef DEBUG_ENABLE
#define oam_pr_debug(file, ...) \
    ( {printf("[DEBUG] "__VA_ARGS__) ; oam_pr_log(file, "[DEBUG] "__VA_ARGS__);} )
#else
#define oam_pr_debug(...)
#endif

#define oam_pr_info(file, ...) \
    ( {printf("[INFO] "__VA_ARGS__) ; oam_pr_log(file, "[INFO] "__VA_ARGS__);} )

#define oam_pr_error(file, ...) \
    ( {fprintf(stderr, "[ERROR] "__VA_ARGS__) ; oam_pr_log(file, "[ERROR] "__VA_ARGS__);})

/* Library interfaces */
const char *netoam_lib_version(void);
oam_session_id oam_session_start(void *params, enum oam_session_type session_type);
void oam_session_stop(oam_session_id session_id);
int oam_get_eth_mac(char *if_name, uint8_t *mac_addr);
int oam_hwaddr_str2bin(char *mac, uint8_t *addr);
int oam_is_eth_vlan(char *if_name);
bool oam_is_frame_tagged(struct msghdr *recv_msg, struct tpacket_auxdata *aux_buf);
void oam_pr_log(char *log_file, const char *format, ...) __attribute__ ((format (gnu_printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif //_LIBNETOAM_H

