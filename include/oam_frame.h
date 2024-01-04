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

#ifndef _OAM_FRAME_H
#define _OAM_FRAME_H

#include <stdint.h>
#include <net/ethernet.h>
#include <libnet.h>
#include <pthread.h>

#include "oam_session.h"
#include "eth-lb.h"
#include "libnetoam.h"

/*
 * OAM OpCodes
 *
 * We only implement ETH-LB for the moment, but let's create an enum
 * and add some more for completion (from IEEE 802.1).
 */
enum oam_opcode {
	OAM_OP_CCM = 1,
	OAM_OP_LBR = 2,
	OAM_OP_LBM = 3,
	OAM_OP_LTR = 4,
	OAM_OP_LTM = 5,
};

/*
 * Generic TLV format
 * In an End TLV, Type = 0, and both Length and Value fields are not used.
 * Below TLV types are from IEEE 802.1.
 */
enum oam_tlv_type {
	OAM_TLV_END = 0,
	OAM_TLV_DATA = 3,
	OAM_TLV_REPLY_INGRESS = 5,
	OAM_TLV_REPLY_EGRESS = 6,
	OAM_TLV_LTM_EGRESS = 7,
	OAM_TLV_LTR_EGRESS = 8,
};

#define ETHERTYPE_OAM 0x8902

/*
 *  Common OAM Header
 *                       octet
 * +------------------+
 * | MEG level        |  1 (high-order 3 bits)
 * +------------------+
 * | Version          |  1 (low-order 5 bits)
 * +------------------+
 * | Opcode           |  2
 * +------------------+
 * | Flags            |  3
 * +------------------+
 * | First TLV Offset |  4
 * +------------------+
 */

struct oam_common_header {
	union {
		uint8_t meg_level;
		uint8_t version;
	} byte1;
	uint8_t opcode;
	uint8_t flags;
	uint8_t tlv_offset;
} __attribute__((__packed__));

/* The rest of the PDU is protocol specific, we only implement ETH-LB for the moment (MAC ping) */

/*
 * ETH-LB PDU is similar for both LBM/LBR frames.
 *
 * There is an optional TLV that could be filled between transaction ID and end tlv,
 * that we currently don't include.
 */
struct oam_lb_pdu {
	struct oam_common_header oam_header;
	uint32_t transaction_id;
	uint8_t end_tlv;
} __attribute__((__packed__));

/* Prototypes */
void oam_build_common_header(uint8_t meg_level, uint8_t version, enum oam_opcode opcode, uint8_t flags,
		uint8_t tlv_offset, struct oam_common_header *header);
void oam_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct oam_lb_pdu *oam_frame);

#endif //_OAM_FRAME_H
