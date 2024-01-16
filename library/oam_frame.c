/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "../include/oam_frame.h"
#include "../include/libnetoam.h"

void oam_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct oam_lb_pdu *oam_frame)
{    
    /* At this point, the common header should be already filled in, so we only add the rest of the LB frame */
    oam_frame->transaction_id = htonl(transaction_id);
    oam_frame->end_tlv = end_tlv;
}

void oam_build_ltm_frame(uint32_t transaction_id, uint8_t ttl, uint8_t *origin_mac, uint8_t *target_mac,
        uint8_t end_tlv, struct oam_ltm_pdu *oam_ltm_frame)
{
    oam_ltm_frame->transaction_id = htonl(transaction_id);
    oam_ltm_frame->ttl = ttl;
    memcpy(oam_ltm_frame->origin_mac, origin_mac, ETH_ALEN);
    memcpy(oam_ltm_frame->target_mac, target_mac, ETH_ALEN);
    oam_ltm_frame->end_tlv = end_tlv;
}

void oam_build_ltr_frame(uint32_t transaction_id, uint8_t ttl, uint8_t relay_action, struct ltr_egress_id_tlv *egress_id,
        struct reply_ingress_tlv *reply_ingress, struct reply_egress_tlv *reply_egress, uint8_t end_tlv,
        struct oam_ltr_pdu *oam_ltr_frame)
{
    oam_ltr_frame->transaction_id = htonl(transaction_id);
    oam_ltr_frame->ttl = ttl;
    oam_ltr_frame->relay_action = relay_action;
    memcpy(&oam_ltr_frame->egress_id, egress_id, sizeof(struct ltr_egress_id_tlv));
    memcpy(&oam_ltr_frame->reply_ingress, reply_ingress, sizeof(struct reply_ingress_tlv));
    memcpy(&oam_ltr_frame->reply_egress, reply_egress, sizeof(struct reply_egress_tlv));
    oam_ltr_frame->end_tlv = end_tlv;
}

void oam_build_common_header(uint8_t meg_level, uint8_t version, enum oam_opcode opcode, uint8_t flags,
        uint8_t tlv_offset, struct oam_common_header *header)
{
    /* MEG level must be in range 0-7 */
    if (meg_level > 7) {
		oam_pr_debug(NULL, "oam_build_common_header: out of range MEG level, setting to 0.\n");
		meg_level = 0;
	}

    header->byte1.version = version;
    header->byte1.meg_level = (meg_level << 5) & 0xe0;
    header->opcode = opcode;
    header->flags = flags;
    header->tlv_offset = tlv_offset;
}