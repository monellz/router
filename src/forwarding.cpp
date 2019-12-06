#include "ip.h"

inline bool checkChecksum(uint8_t *packet, size_t len) {
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
/**
 * @brief 进行转发时所需的 IP 头的更新：
 *        你需要先检查 IP 头校验和的正确性，如果不正确，直接返回 false ；
 *        如果正确，请更新 TTL 和 IP 头校验和，并返回 true 。
 *        你可以从 checksum 题中复制代码到这里使用。
 * @param packet 收到的 IP 包，既是输入也是输出，原地更改
 * @param len 即 packet 的长度，单位为字节
 * @return 校验和无误则返回 true ，有误则返回 false
 */
bool forward(uint8_t *packet, size_t len) {
    uint16_t check_sum = (packet[IP_HEADER_CHECKSUM_0] << 8) | packet[IP_HEADER_CHECKSUM_1];
    if (!checkChecksum(packet, len)) { return false; }

    //update ttl and ip checksum
    uint16_t ttl_protocol = packet[IP_TTL] << 8 | packet[IP_PROTOCOL];
    packet[IP_TTL] -= 0x01;
    uint32_t check_sum_ex = (~check_sum & 0xffff) + (~ttl_protocol & 0xffff) + (packet[IP_TTL] << 8 | packet[IP_PROTOCOL]);
    check_sum = ~((check_sum_ex >> 16) + (check_sum_ex & 0xffff));

    packet[IP_HEADER_CHECKSUM_0] = check_sum >> 8;
    packet[IP_HEADER_CHECKSUM_1] = check_sum & 0xff;
    return true;
}