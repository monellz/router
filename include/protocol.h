#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "rip.h"
#include "udp.h"
#include "icmp.h"
#include "ip.h"

bool disassemble_rip(const uint8_t *packet, uint32_t len, RipPacket *output);
uint32_t assemble_rip(const RipPacket *rip, uint8_t *buffer);
uint32_t assemble_udp(uint8_t *buffer, uint32_t data_len);
uint32_t assemble_icmp(uint8_t *buffer, uint8_t type, uint8_t code, uint8_t *original_ip_packet);
uint32_t assemble_ip(uint8_t *buffer, uint32_t src_addr, uint32_t dst_addr, uint32_t data_len, uint8_t protocol, uint8_t ttl = IP_DEFAULT_TTL);


uint16_t update_ip_checksum(uint8_t *packet, size_t len);
bool validate_ip_checksum(uint8_t *packet, size_t len);


bool forward_packet(uint8_t *packet, size_t len);


#endif