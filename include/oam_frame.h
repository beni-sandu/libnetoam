/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _OAM_FRAME_H
#define _OAM_FRAME_H

#include <stdint.h>
#include <linux/if_ether.h>

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
	OAM_TLV_SENDER_ID = 1,
	OAM_TLV_DATA = 3,
	OAM_TLV_REPLY_INGRESS = 5,
	OAM_TLV_REPLY_EGRESS = 6,
	OAM_TLV_LTM_EGRESS = 7,
	OAM_TLV_LTR_EGRESS = 8,
};

/*
 * Y.1731 does not mention this TLV, but 802.1ag does. In case there is some system that
 * requires for it to be present (although it should be optional), construct a minimum
 * length Sender ID, that is still valid.
 */
struct oam_sender_id_tlv {
	uint8_t type;
	uint16_t length;
	uint8_t chassis_id_len;
} __attribute__((__packed__));

#define ETHERTYPE_OAM	0x8902

/* Complete VLAN header */
struct oam_vlan_header {
	uint8_t dst_addr[ETH_ALEN];
	uint8_t src_addr[ETH_ALEN];
	uint16_t tpi;
	uint16_t pcp_vid;
#define VLAN_VIDMASK	0x0fff
	uint16_t ether_type;
} __attribute__((__packed__));

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

/* Prototypes */
void oam_build_common_header(uint8_t meg_level, uint8_t version, enum oam_opcode opcode, uint8_t flags,
		uint8_t tlv_offset, struct oam_common_header *header);
void oam_build_eth_frame(uint8_t *dst_addr, uint8_t *src_addr, uint16_t type, uint8_t *payload,
		size_t payload_s,uint8_t *frame);
void oam_build_vlan_frame(uint8_t *dst_addr, uint8_t *src_addr, uint16_t tpi, uint8_t pcp, uint8_t dei,
        uint16_t vlan_id, uint16_t ether_type, uint8_t* payload, uint32_t payload_s, uint8_t *frame);

#endif //_OAM_FRAME_H
