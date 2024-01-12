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