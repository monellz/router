#include "ip.h"

/**
 * @brief 进行 IP 头的校验和的验证
 * @param packet 完整的 IP 头和载荷
 * @param len 即 packet 的长度，单位是字节，保证包含完整的 IP 头
 * @return 校验和无误则返回 true ，有误则返回 false
 */
bool validateIPChecksum(uint8_t *packet, size_t len) {
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
    packet[IP_HEADER_CHECKSUM_0] = original_check_sum >> 8;
    packet[IP_HEADER_CHECKSUM_1] = original_check_sum & 0xff;
    return original_check_sum == ~check_sum % 0xffff;
}

uint16_t calculateIPChecksum(uint8_t *packet, size_t len) {
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
    packet[IP_HEADER_CHECKSUM_0] = original_check_sum >> 8;
    packet[IP_HEADER_CHECKSUM_1] = original_check_sum & 0xff;
    return ~check_sum % 0xffff;
}