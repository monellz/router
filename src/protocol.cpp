#include "protocol.h"
#include <cstdio>

bool validate_ip_checksum(uint8_t *packet, size_t len) {
    uint16_t sum = 0;
    size_t header_byte_len = (packet[IP_VERSION_HEADER_LENGTH] & 0x0f) << 2;
    for (int i = 0; i < header_byte_len; i++) {
        sum += packet[i];
    }
    while (sum >> 8) {
        sum = (sum >> 8) + (sum & 0xff);
    }
    return sum == 0xff;
}

uint16_t update_ip_checksum(uint8_t *packet, size_t len) {
    uint16_t original_check_sum = (packet[IP_HEADER_CHECKSUM_0] << 8) | packet[IP_HEADER_CHECKSUM_1];
    size_t header_byte_len = (packet[IP_VERSION_HEADER_LENGTH] & 0x0f) << 2;
    packet[IP_HEADER_CHECKSUM_0] = packet[IP_HEADER_CHECKSUM_1] = 0x00;
    uint32_t check_sum = 0x00000000;
    for (int i = 0;i < header_byte_len - 1; i += 2) {
        check_sum += (packet[i] << 8) | packet[i + 1]; 
    }   
    while (check_sum >> 16) {
        check_sum = (check_sum & 0xffff) + (check_sum >> 16);
    }   
    check_sum = ~check_sum;
    packet[IP_HEADER_CHECKSUM_0] = check_sum >> 8;
    packet[IP_HEADER_CHECKSUM_1] = check_sum & 0xff;
    return check_sum % 0xffff;
}

bool disassemble_rip(const uint8_t *packet, uint32_t len, RipPacket *output) {
    uint16_t total_length = packet[2] << 8 | packet[3];
    uint8_t command = packet[28];
    uint8_t version = packet[29];
    uint16_t zero = packet[30] << 8 | packet[31];
    output->command = command;
    if (total_length <= len && (command == RIP_CMD_REQUEST || command == RIP_CMD_RESPONSE) && version == RIP_VERSION && zero == RIP_ZERO) {
        output->numEntries = (total_length - 28 - 4) / 20;
        uint32_t head = 32;
        for (int i = 0, head = 32;i < output->numEntries; ++i, head += 20) {
            uint8_t af = packet[head] << 8 | packet[head + 1];
            uint32_t tag = packet[head + 2] << 8 | packet[head + 3];
            if ((command == RIP_CMD_REQUEST && af != RIP_AF_REQUEST) || (command == RIP_CMD_RESPONSE && af != RIP_AF_RESPONSE) ||  tag != RIP_TAG) return false;

            output->entries[i].addr = packet[head + 4] | packet[head + 5] << 8 | packet[head + 6] << 16 | packet[head + 7] << 24;
            output->entries[i].mask = packet[head + 8] | packet[head + 9] << 8 | packet[head + 10] << 16 | packet[head + 11] << 24;
            output->entries[i].nexthop = packet[head + 12] | packet[head + 13] << 8 | packet[head + 14] << 16 | packet[head + 15] << 24;
            //uint32_t metric = packet[head + 16] << 24 | packet[head + 17] << 16 | packet[head + 18] << 8 | packet[head + 19];
            output->entries[i].metric = packet[head + 16] << 24 | packet[head + 17] << 16 | packet[head + 18] << 8 | packet[head + 19];

            if (output->entries[i].metric < 1 || output->entries[i].metric > 16) return false;
            if ((output->entries[i].mask + 1) & output->entries[i].mask) return false;
 
        }
        return true;
    } return false;
}

uint32_t assemble_rip(const RipPacket *rip, uint8_t *buffer) {
    //command
    buffer[0] = rip->command;
    //version
    buffer[1] = RIP_VERSION;
    //zero
    buffer[2] = buffer[3] = RIP_ZERO;
    uint16_t af = buffer[0] == RIP_CMD_REQUEST? RIP_AF_REQUEST: RIP_AF_RESPONSE;
    for (int i = 0, head = 4; i< rip->numEntries; ++i, head += 20) {
        buffer[head + 0] = af >> 8;
        buffer[head + 1] = af;
        buffer[head + 2] = buffer[head + 3] = RIP_ZERO;
        buffer[head + 4] = rip->entries[i].addr;
        buffer[head + 5] = rip->entries[i].addr >> 8;
        buffer[head + 6] = rip->entries[i].addr >> 16;
        buffer[head + 7] = rip->entries[i].addr >> 24;
        buffer[head + 8] = rip->entries[i].mask;
        buffer[head + 9] = (rip->entries[i].mask >> 8);
        buffer[head + 10] = (rip->entries[i].mask >> 16);
        buffer[head + 11] = (rip->entries[i].mask >> 24);
        buffer[head + 12] = rip->entries[i].nexthop;
        buffer[head + 13] = rip->entries[i].nexthop >> 8;
        buffer[head + 14] = rip->entries[i].nexthop >> 16;
        buffer[head + 15] = rip->entries[i].nexthop >> 24;
        buffer[head + 16] = rip->entries[i].metric >> 24;
        buffer[head + 17] = rip->entries[i].metric >> 16;
        buffer[head + 18] = rip->entries[i].metric >> 8;
        buffer[head + 19] = rip->entries[i].metric;
    }
    return 4 + 20 * rip->numEntries;
}

uint32_t assemble_udp(uint8_t *buffer, uint32_t data_len) {
    buffer[UDP_SRC_PORT_0] = (UDP_PORT >> 8) & 0xff;
    buffer[UDP_SRC_PORT_1] = UDP_PORT & 0xff;
    buffer[UDP_DST_PORT_0] = (UDP_PORT >> 8) & 0xff;
    buffer[UDP_DST_PORT_1] = UDP_PORT & 0xff;
    buffer[UDP_TOTAL_LENGTH_0] = ((data_len + UDP_DEFAULT_HEADER_LENGTH) >> 8) & 0xff;
    buffer[UDP_TOTAL_LENGTH_1] = (data_len + UDP_DEFAULT_HEADER_LENGTH) & 0xff;
    buffer[UDP_TOTAL_CHECKSUM_0] = buffer[UDP_TOTAL_CHECKSUM_1] = 0;
    return data_len + UDP_DEFAULT_HEADER_LENGTH;
}


uint32_t assemble_ip(uint8_t *buffer, uint32_t src_addr, uint32_t dst_addr, uint32_t data_len, uint8_t protocol, uint8_t ttl) {
    buffer[IP_VERSION_HEADER_LENGTH] = 0x45;
    buffer[IP_TYPE_OF_SERVICE] = 0;
    uint32_t length = IP_DEFAULT_HEADER_LENGTH + data_len;
    buffer[IP_TOTAL_LENGTH_0] = (length >> 8) & 0xff;
    buffer[IP_TOTAL_LENGTH_1] = length & 0xff;
    buffer[IP_IDENTIFIER_0] = buffer[IP_IDENTIFIER_1] = 0;
    buffer[IP_FLAGS_FRAMENTED_OFFSET_0] = buffer[IP_FLAGS_FRAMENTED_OFFSET_1] = 0;
    buffer[IP_TTL_] = ttl;
    buffer[IP_PROTOCOL] = protocol;
    //buffer[IP_HEADER_CHECKSUM_0] = buffer[IP_HEADER_CHECKSUM_1] = 0;
    buffer[IP_SRC_ADDR_0] = src_addr & 0xff;
    buffer[IP_SRC_ADDR_1] = (src_addr >> 8) & 0xff;
    buffer[IP_SRC_ADDR_2] = (src_addr >> 16) & 0xff;
    buffer[IP_SRC_ADDR_3] = (src_addr >> 24) & 0xff;
    buffer[IP_DST_ADDR_0] = dst_addr & 0xff;
    buffer[IP_DST_ADDR_1] = (dst_addr >> 8) & 0xff;
    buffer[IP_DST_ADDR_2] = (dst_addr >> 16) & 0xff;
    buffer[IP_DST_ADDR_3] = (dst_addr >> 24) & 0xff;

    update_ip_checksum(buffer, IP_DEFAULT_HEADER_LENGTH);
    return data_len + IP_DEFAULT_HEADER_LENGTH;
}

uint32_t assemble_icmp(uint8_t *buffer, uint8_t type, uint8_t code, uint8_t *original_ip_packet) {
    buffer[ICMP_TYPE] = type;
    buffer[ICMP_CODE] = code;
    buffer[ICMP_TOTAL_CHECKSUM_0] = buffer[ICMP_TOTAL_CHECKSUM_1] = 0;
    buffer[ICMP_NO_USE_0] = buffer[ICMP_NO_USE_1] = buffer[ICMP_NO_USE_2] = buffer[ICMP_NO_USE_3] = 0;

    //ip
    for (uint32_t i = 0; i < IP_DEFAULT_HEADER_LENGTH + 8; ++i) {
        buffer[ICMP_IP_HEAD + i] = original_ip_packet[i];
    }

    //checksum
    size_t byte_len = 8 + IP_DEFAULT_HEADER_LENGTH + 8;
    uint32_t check_sum = 0x00000000;
    for (int i = 0;i < byte_len - 1; i += 2) {
        check_sum += (buffer[i] << 8) | buffer[i + 1]; 
    }   
    while (check_sum >> 16) {
        check_sum = (check_sum & 0xffff) + (check_sum >> 16);
    }   
    check_sum = ~check_sum;
    buffer[ICMP_TOTAL_CHECKSUM_0] = check_sum >> 8;
    buffer[ICMP_TOTAL_CHECKSUM_1] = check_sum & 0xff;
    return byte_len;
}


bool forward_packet(uint8_t *packet, size_t len) {
    uint16_t check_sum = (packet[IP_HEADER_CHECKSUM_0] << 8) | packet[IP_HEADER_CHECKSUM_1];
    if (!validate_ip_checksum(packet, len)) { return false; }

    //update ttl and ip checksum
    uint16_t ttl_protocol = packet[IP_TTL_] << 8 | packet[IP_PROTOCOL];
    packet[IP_TTL_] -= 0x01;
    uint32_t check_sum_ex = (~check_sum & 0xffff) + (~ttl_protocol & 0xffff) + (packet[IP_TTL_] << 8 | packet[IP_PROTOCOL]);
    check_sum = ~((check_sum_ex >> 16) + (check_sum_ex & 0xffff));

    packet[IP_HEADER_CHECKSUM_0] = check_sum >> 8;
    packet[IP_HEADER_CHECKSUM_1] = check_sum & 0xff;
    return true;
}
