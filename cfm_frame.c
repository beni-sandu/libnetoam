/*
 * Copyright (C) 2022 Beniamin Sandu <beniaminsandu@gmail.com>
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

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "cfm_frame.h"

void cfm_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct cfm_lb_pdu *cfm_frame) {
    
    /* At this point, the common header should be already filled in, so we only add the rest of the LB frame */
    cfm_frame->transaction_id = htonl(transaction_id);
    cfm_frame->end_tlv = end_tlv;
}

void cfm_build_common_header(uint8_t md_level, uint8_t version, enum cfm_opcode opcode, uint8_t flags,
        uint8_t tlv_offset, struct cfm_common_header *header) {

    /* MD level must be in range 0-7 */
    if (md_level > 7) {
		fprintf(stderr, "cfm_build_common_header: out of range MD level, setting to 0.\n");
		md_level = 0;
	}

    header->byte1.version = 0;
    header->byte1.md_level = (md_level << 5) & 0xe0;
    header->opcode = opcode;
    header->flags = flags;
    header->tlv_offset = tlv_offset;
}