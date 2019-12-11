#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>


#include "router_hal.h"
#include "route_table.h"
#include "protocol.h"

#define REGULAR_TIMER   5
uint8_t packet[2048];
uint8_t output[2048];

//in_addr_t addrs[N_IFACE_ON_BOARD] = {0x0100000a, 0x0101000a, 0x0102000a, 0x0103000a};
//in_addr_t addrs[N_IFACE_ON_BOARD] = {0xf31bfea9, 0x0303000a, 0x0203000a, 0x0103000a};
in_addr_t addrs[N_IFACE_ON_BOARD] = {0x21002a0a, 0x0303000a, 0x0203000a, 0x0103000a};
uint32_t addrs_len[N_IFACE_ON_BOARD] = {16, 24, 24, 24};

void trigger_one(uint32_t addr, uint32_t mask_len, uint32_t nexthop, uint32_t if_index, uint32_t metric) {
    route_print(addr, mask_len, nexthop, if_index, metric, "trigger one");
	RipPacket resp;
	resp.numEntries = 1;
	resp.command = RIP_CMD_RESPONSE;
	resp.entries[0].addr = addr;
	resp.entries[0].mask = mask_right(mask_len);
	resp.entries[0].metric = metric;
	resp.entries[0].nexthop = nexthop;
	uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
	uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
    macaddr_t multicast_dst;
    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        uint32_t ip_len = assemble_ip(output, addrs[i], RIP_MULTICAST_ADDR, udp_len, IP_PROTOCOL_UDP, 1);
        HAL_ArpGetMacAddress(i, RIP_MULTICAST_ADDR, multicast_dst);
        HAL_SendIPPacket(i, output, ip_len, multicast_dst);   
    }
}

void trigger_all() {
    route_print_all("trigger all");
    RipPacket resp;
    route_fill_rip_packet(&resp);
    uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
    uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
    macaddr_t multicast_dst;
    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        uint32_t ip_len = assemble_ip(output, addrs[i], RIP_MULTICAST_ADDR, udp_len, IP_PROTOCOL_UDP, 1);
        HAL_ArpGetMacAddress(i, RIP_MULTICAST_ADDR, multicast_dst);
        HAL_SendIPPacket(i, output, ip_len, multicast_dst);   
    }
}

void request() {
    printf("Request for all interfaces\n");
    RipPacket resp;
    resp.numEntries = 0;
    resp.command = RIP_CMD_REQUEST;
    uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
    uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
    macaddr_t multicast_dst;
    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        uint32_t ip_len = assemble_ip(output, addrs[i], RIP_MULTICAST_ADDR, udp_len, IP_PROTOCOL_UDP, 1);
        HAL_ArpGetMacAddress(i, RIP_MULTICAST_ADDR, multicast_dst);
        HAL_SendIPPacket(i, output, ip_len, multicast_dst);   
    }
}

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
        route_insert(addrs[i], addrs_len[i], i, 0, 1);
    }

    //Request
    request();


    uint64_t last_time = 0;
    uint64_t trigger_last_time = 0;
    while (1) {
        uint64_t time = HAL_GetTicks();
        if (time > last_time + REGULAR_TIMER * 1000) {
            // What to do?
            // send complete routing table to every interface
            // ref. RFC2453 3.8
            printf("%ds Timer, regular Response for all interfaces\n", REGULAR_TIMER);
            trigger_all();
            trigger_last_time = last_time = time;
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
        if (!validate_ip_checksum(packet, res)) {
            printf("Invalid IP Checksum\n");
            continue;
        }
        // extract src_addr and dst_addr from packet
        // big endian
        in_addr_t src_addr, dst_addr;
        src_addr = packet[IP_SRC_ADDR_0] | packet[IP_SRC_ADDR_1] << 8 | packet[IP_SRC_ADDR_2] << 16 | packet[IP_SRC_ADDR_3] << 24;
        dst_addr = packet[IP_DST_ADDR_0] | packet[IP_DST_ADDR_1] << 8 | packet[IP_DST_ADDR_2] << 16 | packet[IP_DST_ADDR_3] << 24;
        //printf("Receive packet src = %08x, dst = %08x\n", src_addr, dst_addr);


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
            if (disassemble_rip(packet, res, &rip)) {
                if (rip.command == RIP_CMD_REQUEST) {
                    printf("Receive Request from %d.%d.%d.%d to %d.%d.%d.%d\n", IPFORMAT(src_addr), IPFORMAT(dst_addr));
                    // 3a.3 request, ref. RFC2453 3.9.1
                    // only need to respond to whole table requests in the lab
                    RipPacket resp;
                    // TODO: fill resp
                    route_fill_rip_packet(&resp);
                    uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
                    uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
                    uint32_t ip_len = assemble_ip(output, addrs[if_index], src_addr, udp_len, IP_PROTOCOL_UDP, 1);
                    // send it back
                    printf("Send Response to %d.%d.%d.%d\n", IPFORMAT(src_addr));
                    HAL_SendIPPacket(if_index, output, ip_len, src_mac);
                } else {
                    printf("Receive Response from %d.%d.%d.%d to %d.%d.%d.%d, entry_num = %d\n", IPFORMAT(src_addr), IPFORMAT(dst_addr), rip.numEntries);
                    // 3a.2 response, ref. RFC2453 3.9.2
                    // update routing table
                    // new metric = ?
                    // update metric, if_index, nexthop
                    // what is missing from RoutingTableEntry?
                    // TODO: use query and update
                    // triggered updates? ref. RFC2453 3.10.1
                    uint32_t query_nexthop, query_if_index, query_metric;
                    for (uint32_t i = 0; i < rip.numEntries; ++i) {
                        bool is_time_to_trigger = HAL_GetTicks() > trigger_last_time + 1 * 1000;
                        uint32_t new_metric = rip.entries[i].metric + 1 >= RIP_METRIC_INFINITY ? RIP_METRIC_INFINITY: rip.entries[i].metric + 1;
                        uint32_t mask_len = mask_len_from_mask_right(rip.entries[i].mask);
                        //printf("prepare to query %d.%d.%d.%d\n", IPFORMAT(rip.entries[i].addr));
                        if (route_query(rip.entries[i].addr, &query_nexthop, &query_if_index, &query_metric)) {
                            //printf("queried!, metric = %d\n", query_metric);
                            if ((query_nexthop == src_addr && new_metric != query_metric) || new_metric < query_metric) {
                                //printf("insert, query_addr = %d.%d.%d.%d, query_nexthop = %d.%d.%d.%d, src_addr = %d.%d.%d.%d, new_metric = %d, query_metric = %d\n", IPFORMAT(rip.entries[i].addr), IPFORMAT(query_nexthop), IPFORMAT(src_addr), new_metric, query_metric);
                                route_insert(rip.entries[i].addr, mask_len, if_index, src_addr, new_metric);
                                if (is_time_to_trigger) {
                                    trigger_one(rip.entries[i].addr, mask_len, src_addr, if_index, new_metric);
                                    trigger_last_time = HAL_GetTicks();
                                }
                            }
                        } else {
                            //printf("NOT queried!\n");
                            if (new_metric < RIP_METRIC_INFINITY) {
                                //printf("not queried, but can insert for %d.%d.%d.%d\n", IPFORMAT(rip.entries[i].addr));
                                route_insert(rip.entries[i].addr, mask_len, if_index, src_addr, new_metric);
                                if (is_time_to_trigger) {
                                    trigger_one(rip.entries[i].addr, mask_len, src_addr, if_index, new_metric);
                                    trigger_last_time = HAL_GetTicks();
                                }
                            }
                        }
                    }
                }
            }
        } else {
            printf("Forward, Packet is from %d.%d.%d.%d:%d to %d.%d.%d.%d\n", IPFORMAT(src_addr), if_index, IPFORMAT(dst_addr));
            // 3b.1 dst is not me
            // forward
            // beware of endianness
            uint32_t nexthop, dest_if, metric;
            if (route_query(dst_addr, &nexthop, &dest_if, &metric)) {
                //found
                macaddr_t dest_mac;
                if (nexthop == 0) nexthop = dst_addr;
                if (HAL_ArpGetMacAddress(dest_if, nexthop, dest_mac) == 0) {
                    //found mac addr
                    memcpy(output, packet, res);
                    forward_packet(output, res);

                    //check ttl = 0
                    if (output[IP_TTL_] == 0) {
                        printf("Packet TTL = 0\nSend ICMP Time Exceeded to %d.%d.%d.%d\n", IPFORMAT(src_addr));
                        
                        uint32_t icmp_len = assemble_icmp(packet + IP_DEFAULT_HEADER_LENGTH, ICMP_TYPE_TIME_EXCEEDED, ICMP_CODE_FRAGMENT_REASSEMBLY_TIME_EXCEEDED, output);
                        uint32_t ip_len = assemble_ip(packet, addrs[if_index], src_addr, icmp_len, IP_PROTOCOL_ICMP);
                        HAL_SendIPPacket(if_index, packet, ip_len, src_mac);
                    } else {
                        HAL_SendIPPacket(dest_if, output, res, dest_mac);
                    }
                } else {
                    printf("ARP not found for %d.%d.%d.%d, throw away\n", IPFORMAT(nexthop));
                }
            } else {
                printf("IP network not found for %d.%d.%d.%d\n", IPFORMAT(dst_addr));
                printf("Send ICMP Destination Network Unreachable to %d.%d.%d.%d\n", IPFORMAT(src_addr));
                uint32_t icmp_len = assemble_icmp(output + IP_DEFAULT_HEADER_LENGTH, ICMP_TYPE_DEST_UNREACHABLE, ICMP_CODE_NETWORK_UNREACHABLE, packet);
                uint32_t ip_len = assemble_ip(packet, addrs[if_index], src_addr, icmp_len, IP_PROTOCOL_ICMP);
                HAL_SendIPPacket(if_index, packet, ip_len, src_mac);
            }
        }
    }
    return 0;
}
