#include "rip.h"
#include "udp.h"
#include "ip.h"

extern uint16_t updateIPChecksum(uint8_t *packet, size_t len);
/*
  在头文件 rip.h 中定义了如下的结构体：
  #define RIP_MAX_ENTRY 25
  typedef struct {
    // all fields are big endian
    // we don't store 'family', as it is always 2(for response) and 0(for request)
    // we don't store 'tag', as it is always 0
    uint32_t addr;
    uint32_t mask;
    uint32_t nexthop;
    uint32_t metric;
  } RipEntry;

  typedef struct {
    uint32_t numEntries;
    // all fields below are big endian
    uint8_t command; // 1 for request, 2 for response, otherwsie invalid
    // we don't store 'version', as it is always 2
    // we don't store 'zero', as it is always 0
    RipEntry entries[RIP_MAX_ENTRY];
  } RipPacket;

  你需要从 IPv4 包中解析出 RipPacket 结构体，也要从 RipPacket 结构体构造出对应的 IP 包
  由于 Rip 包结构本身不记录表项的个数，需要从 IP 头的长度中推断，所以在 RipPacket 中额外记录了个数。
  需要注意这里的地址都是用 **大端序** 存储的，1.2.3.4 对应 0x04030201 。
*/

/**
 * @brief 从接受到的 IP 包解析出 Rip 协议的数据
 * @param packet 接受到的 IP 包
 * @param len 即 packet 的长度
 * @param output 把解析结果写入 *output
 * @return 如果输入是一个合法的 RIP 包，把它的内容写入 RipPacket 并且返回 true；否则返回 false
 * 
 * IP 包的 Total Length 长度可能和 len 不同，当 Total Length 大于 len 时，把传入的 IP 包视为不合法。
 * 你不需要校验 IP 头和 UDP 的校验和是否合法。
 * 你需要检查 Command 是否为 1 或 2，Version 是否为 2， Zero 是否为 0，
 * Family 和 Command 是否有正确的对应关系（见上面结构体注释），Tag 是否为 0，
 * Metric 转换成小端序后是否在 [1,16] 的区间内，
 * Mask 的二进制是不是连续的 1 与连续的 0 组成等等。
 */
#include <stdio.h>
bool disassembleRIP(const uint8_t *packet, uint32_t len, RipPacket *output) {
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
    } else return false;
}

/**
 * @brief 从 RipPacket 的数据结构构造出 RIP 协议的二进制格式
 * @param rip 一个 RipPacket 结构体
 * @param buffer 一个足够大的缓冲区，你要把 RIP 协议的数据写进去
 * @return 写入 buffer 的数据长度
 * 
 * 在构造二进制格式的时候，你需要把 RipPacket 中没有保存的一些固定值补充上，包括 Version、Zero、Address Family 和 Route Tag 这四个字段
 * 你写入 buffer 的数据长度和返回值都应该是四个字节的 RIP 头，加上每项 20 字节。
 * 需要注意一些没有保存在 RipPacket 结构体内的数据的填写。
 */
uint32_t assembleRIP(const RipPacket *rip, uint8_t *buffer) {
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
        buffer[head + 8] = ~rip->entries[i].mask;
        buffer[head + 9] = ~(rip->entries[i].mask >> 8);
        buffer[head + 10] = ~(rip->entries[i].mask >> 16);
        buffer[head + 11] = ~(rip->entries[i].mask >> 24);
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

uint32_t assembleUDP(uint8_t *buffer, uint32_t data_len) {
    buffer[UDP_SRC_PORT_0] = (UDP_PORT >> 8) & 0xff;
    buffer[UDP_SRC_PORT_1] = UDP_PORT & 0xff;
    buffer[UDP_DST_PORT_0] = (UDP_PORT >> 8) & 0xff;
    buffer[UDP_DST_PORT_1] = UDP_PORT & 0xff;
    buffer[UDP_TOTAL_LENGTH_0] = ((data_len + UDP_DEFAULT_HEADER_LENGTH) >> 8) & 0xff;
    buffer[UDP_TOTAL_LENGTH_1] = (data_len + UDP_DEFAULT_HEADER_LENGTH) & 0xff;
    buffer[UDP_TOTAL_CHECKSUM_0] = buffer[UDP_TOTAL_CHECKSUM_1] = 0;
    return data_len + UDP_DEFAULT_HEADER_LENGTH;
}


uint32_t assembleIP(uint8_t *buffer, uint32_t src_addr, uint32_t dst_addr, uint32_t data_len) {
    buffer[IP_VERSION_HEADER_LENGTH] = 0x45;
    buffer[IP_TYPE_OF_SERVICE] = 0;
    uint32_t length = IP_DEFAULT_HEADER_LENGTH + data_len;
    buffer[IP_TOTAL_LENGTH_0] = (length >> 8) & 0xff;
    buffer[IP_TOTAL_LENGTH_1] = length & 0xff;
    buffer[IP_IDENTIFIER_0] = buffer[IP_IDENTIFIER_1] = 0;
    buffer[IP_FLAGS_FRAMENTED_OFFSET_0] = buffer[IP_FLAGS_FRAMENTED_OFFSET_1] = 0;
    buffer[IP_TTL] = 1;
    buffer[IP_PROTOCOL] = IP_PROTOCOL_UDP;
    //buffer[IP_HEADER_CHECKSUM_0] = buffer[IP_HEADER_CHECKSUM_1] = 0;
    buffer[IP_SRC_ADDR_0] = src_addr & 0xff;
    buffer[IP_SRC_ADDR_1] = (src_addr >> 8) & 0xff;
    buffer[IP_SRC_ADDR_2] = (src_addr >> 16) & 0xff;
    buffer[IP_SRC_ADDR_3] = (src_addr >> 24) & 0xff;
    buffer[IP_DST_ADDR_0] = dst_addr & 0xff;
    buffer[IP_DST_ADDR_1] = (dst_addr >> 8) & 0xff;
    buffer[IP_DST_ADDR_2] = (dst_addr >> 16) & 0xff;
    buffer[IP_DST_ADDR_3] = (dst_addr >> 24) & 0xff;

    updateIPChecksum(buffer, IP_DEFAULT_HEADER_LENGTH);
    return data_len + IP_DEFAULT_HEADER_LENGTH;
}
