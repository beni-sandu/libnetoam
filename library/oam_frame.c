/*
 * Copyright: Beniamin Sandu <beniaminsandu@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>

#include "../include/oam_frame.h"
#include "../include/libnetoam.h"

void oam_build_eth_frame(uint8_t *dst_addr, uint8_t *src_addr, uint16_t type, uint8_t *payload, size_t payload_s,
        uint8_t *frame)
{
    struct ether_header eth_hdr;

    /* Fill in header */
    memset(&eth_hdr, 0, ETHER_HDR_LEN);
    memcpy(&eth_hdr.ether_dhost, dst_addr, ETH_ALEN);
    memcpy(&eth_hdr.ether_shost, src_addr, ETH_ALEN);
    eth_hdr.ether_type = htons(type);

    /* Fill in final frame */
    memcpy(frame, &eth_hdr, ETHER_HDR_LEN);
    memcpy(frame + ETHER_HDR_LEN, payload, payload_s);
}

void oam_build_vlan_frame(const uint8_t *dst_addr, const uint8_t *src_addr, uint16_t tpi, uint8_t pcp, uint8_t cfi,
        uint16_t vlan_id, uint16_t ether_type, uint8_t* payload, uint32_t payload_s, uint8_t *frame)
{
    struct oam_vlan_header vlan_hdr;

    /* Fill in header */
    memset(&vlan_hdr, 0, sizeof(struct oam_vlan_header));
    memcpy(&vlan_hdr.dst_addr, dst_addr, ETH_ALEN);
    memcpy(&vlan_hdr.src_addr, src_addr, ETH_ALEN);
    vlan_hdr.tpi = htons(tpi);
    vlan_hdr.pcp_vid = htons((pcp << 13) | (cfi << 12) | (vlan_id & VLAN_VIDMASK));
    vlan_hdr.ether_type = htons(ether_type);

    /* Fill in final frame */
    memcpy(frame, &vlan_hdr, sizeof(struct oam_vlan_header));
    memcpy(frame + sizeof(struct oam_vlan_header), payload, payload_s);
}

void oam_build_lb_frame(uint32_t transaction_id, uint8_t end_tlv, struct oam_lb_pdu *oam_frame)
{    
    /* At this point, the common header should be already filled in, so we only add the rest of the LB frame */
    oam_frame->transaction_id = htonl(transaction_id);

    /* Add Sender ID TLV */
    oam_frame->sender_id.type = OAM_TLV_SENDER_ID;
    oam_frame->sender_id.length = htons(1);
    oam_frame->sender_id.chassis_id_len = 0;

    /* Add End TLV */
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