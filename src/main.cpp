#include "rip.h"
#include "ip.h"
#include "udp.h"
#include "router.h"
#include "router_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool validateIPChecksum(uint8_t *packet, size_t len);
extern void update(bool insert, RoutingTableEntry entry);
extern bool query(uint32_t addr, uint32_t *nexthop, uint32_t *if_index);
extern bool forward(uint8_t *packet, size_t len);
extern bool disassembleRIP(const uint8_t *packet, uint32_t len, RipPacket *output);
extern uint32_t assembleRIP(const RipPacket *rip, uint8_t *buffer);
extern uint32_t assembleUDP(uint8_t *buffer, uint32_t data_len);
extern uint32_t assembleIP(uint8_t *buffer, uint32_t src_addr, uint32_t dst_addr, uint32_t data_len);

void fillRipPacket(RipPacket *packet) {

}


uint8_t packet[2048];
uint8_t output[2048];
// 0: 10.0.0.1
// 1: 10.0.1.1
// 2: 10.0.2.1
// 3: 10.0.3.1
// 你可以按需进行修改，注意端序
in_addr_t addrs[N_IFACE_ON_BOARD] = {0x0100000a, 0x0101000a, 0x0102000a, 0x0103000a};

int main(int argc, char *argv[]) {
    // 0a.
    int res = HAL_Init(1, addrs);
    if (res < 0) {
        return res;
    }

    // 0b. Add direct routes
    // For example:
    // 10.0.0.0/24 if 0
    // 10.0.1.0/24 if 1
    // 10.0.2.0/24 if 2
    // 10.0.3.0/24 if 3
    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        RoutingTableEntry entry = {
            .addr = addrs[i], // big endian
            .len = 24,        // small endian
            .if_index = i,    // small endian
            .nexthop = 0      // big endian, means direct
        };
        update(true, entry);
    }

    uint64_t last_time = 0;
    while (1) {
        uint64_t time = HAL_GetTicks();
        if (time > last_time + 30 * 1000) {
            // What to do?
            // send complete routing table to every interface
            // ref. RFC2453 3.8
            printf("30s Timer, Send Response for all interfaces\n");
            for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
                
            }

            last_time = time;
        }

        int mask = (1 << N_IFACE_ON_BOARD) - 1;
        macaddr_t src_mac;
        macaddr_t dst_mac;
        int if_index;
        res = HAL_ReceiveIPPacket(mask, packet, sizeof(packet), src_mac, dst_mac, 1000, &if_index);
        if (res == HAL_ERR_EOF) {
            break;
        } else if (res < 0) {
            return res;
        } else if (res == 0) {
            // Timeout
            continue;
        } else if (res > sizeof(packet)) {
            // packet is truncated, ignore it
            continue;
        }

        // 1. validate
        if (!validateIPChecksum(packet, res)) {
            printf("Invalid IP Checksum\n");
            continue;
        }
        // extract src_addr and dst_addr from packet
        // big endian
        in_addr_t src_addr, dst_addr;
        src_addr = packet[IP_SRC_ADDR_0] | packet[IP_SRC_ADDR_1] << 8 | packet[IP_SRC_ADDR_2] << 16 | packet[IP_SRC_ADDR_3] << 24;
        dst_addr = packet[IP_DST_ADDR_0] | packet[IP_DST_ADDR_1] << 8 | packet[IP_DST_ADDR_2] << 16 | packet[IP_DST_ADDR_3] << 24;


        // 2. check whether dst is me
        bool dst_is_me = false;
        for (int i = 0; i < N_IFACE_ON_BOARD; i++) {
            if (memcmp(&dst_addr, &addrs[i], sizeof(in_addr_t)) == 0) {
                dst_is_me = true;
                break;
            }
        }
        // TODO: Handle rip multicast address(224.0.0.9)?
        bool dst_is_multicast = dst_addr == RIP_MULTICAST_ADDR;

        if (dst_is_me || dst_is_multicast) {
            // 3a.1
            RipPacket rip;
            // check and validate
            if (disassembleRIP(packet, res, &rip)) {
                if (rip.command == RIP_CMD_REQUEST) {
                    // 3a.3 request, ref. RFC2453 3.9.1
                    // only need to respond to whole table requests in the lab
                    RipPacket resp;
                    // TODO: fill resp
                    fillRipPacket(&resp);
                    // assemble
                    uint32_t rip_len = assembleRIP(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
                    uint32_t udp_len = assembleUDP(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
                    uint32_t ip_len = assembleIP(output, dst_addr, src_addr, udp_len);
                    // send it back
                    HAL_SendIPPacket(if_index, output, ip_len, src_mac);
                } else {
                    // 3a.2 response, ref. RFC2453 3.9.2
                    // update routing table
                    // new metric = ?
                    // update metric, if_index, nexthop
                    // what is missing from RoutingTableEntry?
                    // TODO: use query and update
                    // triggered updates? ref. RFC2453 3.10.1
                }
            }
        } else {
            // 3b.1 dst is not me
            // forward
            // beware of endianness
            uint32_t nexthop, dest_if;
            if (query(dst_addr, &nexthop, &dest_if)) {
                // found
                macaddr_t dest_mac;
                // direct routing
                if (nexthop == 0) {
                    nexthop = dst_addr;
                }
                if (HAL_ArpGetMacAddress(dest_if, nexthop, dest_mac) == 0) {
                    // found
                    memcpy(output, packet, res);
                    // update ttl and checksum
                    forward(output, res);
                    // TODO: you might want to check ttl=0 case
                    HAL_SendIPPacket(dest_if, output, res, dest_mac);
                } else {
                    // not found
                    // you can drop it
                    printf("ARP not found for %x\n", nexthop);
                }
            } else {
                // not found
                // optionally you can send ICMP Host Unreachable
                printf("IP not found for %x\n", src_addr);
            }
        }
    }
    return 0;
}
