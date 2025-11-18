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

#include <linux/if_packet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "oam_session.h"
#include "eth_lb.h"

/* Library version */
#define LIBNETOAM_VERSION "0.1.2"

/* Print macros */
#ifdef DEBUG_ENABLE
#define oam_pr_debug(param_ptr, ...) \
    ({ \
    if (param_ptr == NULL) \
        printf("[DEBUG] "__VA_ARGS__); \
    else {\
        if (((struct oam_lb_session_params *)param_ptr)->log_utc == true) \
            oam_pr_log_utc(((struct oam_lb_session_params *)param_ptr)->log_file, "[DEBUG] "__VA_ARGS__); \
        else \
            oam_pr_log(((struct oam_lb_session_params *)param_ptr)->log_file, "[DEBUG] "__VA_ARGS__); \
        if (((struct oam_lb_session_params *)param_ptr)->enable_console_logs == true) \
            printf("[DEBUG] "__VA_ARGS__); \
    } \
    })
#else
#define oam_pr_debug(...) \
    ({ do {} while(0); })
#endif

#define oam_pr_info(param_ptr, ...) \
    ({ \
    if (param_ptr == NULL) \
        printf("[INFO] "__VA_ARGS__); \
    else {\
        if (((struct oam_lb_session_params *)param_ptr)->log_utc == true) \
            oam_pr_log_utc(((struct oam_lb_session_params *)param_ptr)->log_file, "[INFO] "__VA_ARGS__); \
        else \
            oam_pr_log(((struct oam_lb_session_params *)param_ptr)->log_file, "[INFO] "__VA_ARGS__); \
        if (((struct oam_lb_session_params *)param_ptr)->enable_console_logs == true) \
            printf("[INFO] "__VA_ARGS__); \
    } \
    })

#define oam_pr_error(param_ptr, ...) \
    ({ \
    if (param_ptr == NULL) \
        fprintf(stderr, "[ERROR] "__VA_ARGS__); \
    else {\
        if (((struct oam_lb_session_params *)param_ptr)->log_utc == true) \
            oam_pr_log_utc(((struct oam_lb_session_params *)param_ptr)->log_file, "[ERROR] "__VA_ARGS__); \
        else \
            oam_pr_log(((struct oam_lb_session_params *)param_ptr)->log_file, "[ERROR] "__VA_ARGS__); \
        if (((struct oam_lb_session_params *)param_ptr)->enable_console_logs == true) \
            fprintf(stderr, "[ERROR] "__VA_ARGS__); \
    } \
    })

/* Library interfaces */
const char *netoam_lib_version(void);
oam_session_id oam_session_start(void *params, enum oam_session_type session_type);
void oam_session_stop(oam_session_id session_id);
int oam_get_eth_mac(char *if_name, uint8_t *mac_addr, struct oam_lb_session *oam_session);
int oam_hwaddr_str2bin(const char *mac, uint8_t *addr);
int oam_is_eth_vlan(char *if_name, struct oam_lb_session *oam_session);
bool oam_is_frame_tagged(struct msghdr *recv_msg, struct tpacket_auxdata *aux_buf);
void oam_pr_log(char *log_file, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void oam_pr_log_utc(char *log_file, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
char *oam_perror(int error);

#ifdef __cplusplus
}
#endif

#endif //_LIBNETOAM_H

