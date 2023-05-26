/*
 * Copyright (C) 2023 Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LIBNETOAM_H
#define _LIBNETOAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <linux/if_packet.h>

#include "oam_session.h"
#include "eth-lb.h"

/* Library version */
#define LIBNETOAM_VERSION "0.1"

/* Flag to enable debug messages */
#ifdef DEBUG_ENABLE
#define pr_debug(file, ...) \
    ( {printf("[DEBUG] "__VA_ARGS__) ; pr_log(file, "[DEBUG] "__VA_ARGS__);} )
#else
#define pr_debug(...)
#endif

/* Library interfaces */
const char *netoam_lib_version(void);
oam_session_id oam_session_start(void *params, enum oam_session_type session_type);
void oam_session_stop(oam_session_id session_id);
int get_eth_mac(char *if_name, uint8_t *mac_addr);
int hwaddr_str2bin(char *mac, uint8_t *addr);
bool is_eth_vlan(char *if_name);
bool is_frame_tagged(struct msghdr *recv_msg, struct tpacket_auxdata *aux_buf);
void pr_log(char *log_file, const char *format, ...) __attribute__ ((format (gnu_printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif //_LIBNETOAM_H

