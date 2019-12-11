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

//in_addr_t addrs[N_IFACE_ON_BOARD] = {0x0201a8c0, 0x0102a8c0, 0x0203000a, 0x0103000a};
in_addr_t addrs[N_IFACE_ON_BOARD] = {0x0203a8c0, 0x0104a8c0, 0x0205000a, 0x0104000a};
//uint32_t addrs_len[N_IFACE_ON_BOARD] = {16, 24, 24, 24};
//uint32_t addrs_len[N_IFACE_ON_BOARD] = {24, 24, 24, 24};

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
        if (if_index == i) continue;
        uint32_t ip_len = assemble_ip(output, addrs[i], RIP_MULTICAST_ADDR, udp_len, IP_PROTOCOL_UDP, 1);
        HAL_ArpGetMacAddress(i, RIP_MULTICAST_ADDR, multicast_dst);
        HAL_SendIPPacket(i, output, ip_len, multicast_dst);   
    }
}

void trigger_all() {
    route_print_all("trigger all");
    RipPacket resp;
    macaddr_t multicast_dst;
    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        route_fill_rip_packet(&resp, i);
        uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
        uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
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
    int res = HAL_Init(1, addrs);
    if (res < 0) {
        return res;
    }

    for (uint32_t i = 0; i < N_IFACE_ON_BOARD; i++) {
        //route_insert(addrs[i] & 0x00ffffff, addrs_len[i], i, 0, 1);
        route_insert(addrs[i] & 0x00ffffff, 24, i, 0, 1);
    }

    //Request when start
    request();


    uint64_t last_time = 0;
    uint64_t trigger_last_time = 0;
    while (1) {
        uint64_t time = HAL_GetTicks();
        if (time > last_time + REGULAR_TIMER * 1000) {
            // send routing table to every interface
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
            printf("HAL_ERR_EOF\n");
            break;
        } else if (res < 0) {
            printf("Receive res < 0\n");
            return res;
        } else if (res == 0) {
            printf("Receive timeout\n");
            continue;
        } else if (res > sizeof(packet)) {
            printf("Received packet is trucated, ignore\n");
            continue;
        }

        if (!validate_ip_checksum(packet, res)) {
            printf("Invalid IP Checksum\n");
            continue;
        }
        // extract src_addr and dst_addr from packet
        // big endian
        in_addr_t src_addr, dst_addr;
        src_addr = packet[IP_SRC_ADDR_0] | packet[IP_SRC_ADDR_1] << 8 | packet[IP_SRC_ADDR_2] << 16 | packet[IP_SRC_ADDR_3] << 24;
        dst_addr = packet[IP_DST_ADDR_0] | packet[IP_DST_ADDR_1] << 8 | packet[IP_DST_ADDR_2] << 16 | packet[IP_DST_ADDR_3] << 24;
        printf("Receive validated packet src = %d.%d.%d.%d, dst = %d.%d.%d.%d\n", IPFORMAT(src_addr), IPFORMAT(dst_addr));


        //check whether dst is me
        bool dst_is_me = false;
        for (int i = 0; i < N_IFACE_ON_BOARD; i++) {
            if (memcmp(&dst_addr, &addrs[i], sizeof(in_addr_t)) == 0) {
                dst_is_me = true;
                break;
            }
        }
        bool dst_is_multicast = dst_addr == RIP_MULTICAST_ADDR;
        if (dst_is_me || dst_is_multicast) {
            RipPacket rip;
            // check and validate
            if (disassemble_rip(packet, res, &rip)) {
                if (rip.command == RIP_CMD_REQUEST) {
                    printf("RIP Request from %d.%d.%d.%d to %d.%d.%d.%d\n", IPFORMAT(src_addr), IPFORMAT(dst_addr));
                    // only need to respond to whole table requests in the lab
                    RipPacket resp;
                    route_fill_rip_packet(&resp, if_index);
                    uint32_t rip_len = assemble_rip(&resp, output + IP_DEFAULT_HEADER_LENGTH + UDP_DEFAULT_HEADER_LENGTH);
                    uint32_t udp_len = assemble_udp(output + IP_DEFAULT_HEADER_LENGTH, rip_len);
                    uint32_t ip_len = assemble_ip(output, addrs[if_index], src_addr, udp_len, IP_PROTOCOL_UDP, 1);
                    // send it back
                    printf("Send Response to %d.%d.%d.%d\n", IPFORMAT(src_addr));
                    HAL_SendIPPacket(if_index, output, ip_len, src_mac);
                } else {
                    printf("RIP Response from %d.%d.%d.%d to %d.%d.%d.%d, entry_num = %d\n", IPFORMAT(src_addr), IPFORMAT(dst_addr), rip.numEntries);
                    // update routing table
                    uint32_t query_nexthop, query_if_index, query_metric;
                    for (uint32_t i = 0; i < rip.numEntries; ++i) {
                        bool is_time_to_trigger = HAL_GetTicks() > trigger_last_time + 1 * 1000;
                        uint32_t new_metric = (rip.entries[i].metric + 1 >= RIP_METRIC_INFINITY)? RIP_METRIC_INFINITY: rip.entries[i].metric + 1;
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
            } else {
                printf("Warning, assemble RIP failed\n");
            }
        } else {
            printf("Forward, Packet is from %d.%d.%d.%d:%d to %d.%d.%d.%d\n", IPFORMAT(src_addr), if_index, IPFORMAT(dst_addr));
            // forward
            uint32_t nexthop, dest_if, metric;
            if (route_query(dst_addr, &nexthop, &dest_if, &metric)) {
                //found
                printf("Route found dst_addr=%d.%d.%d.%d, nexthop=%d.%d.%d.%d, dest_if=%d, metric=%d\n", IPFORMAT(dst_addr), IPFORMAT(nexthop), dest_if, metric);
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
                        printf("Forward send to %d.%d.%d.%d, if_index is %d\n",IPFORMAT(dst_addr), dest_if); 
                        HAL_SendIPPacket(dest_if, output, res, dest_mac);
                    }
                } else {
                    printf("ARP not found for %d.%d.%d.%d, throw away\n", IPFORMAT(nexthop));
                }
            } else {
                printf("IP network not found for %d.%d.%d.%d\n", IPFORMAT(dst_addr));
                printf("Send ICMP Destination Network Unreachable to %d.%d.%d.%d\n", IPFORMAT(src_addr));
                uint32_t icmp_len = assemble_icmp(output + IP_DEFAULT_HEADER_LENGTH, ICMP_TYPE_DEST_UNREACHABLE, ICMP_CODE_NETWORK_UNREACHABLE, packet);
                uint32_t ip_len = assemble_ip(output, addrs[if_index], src_addr, icmp_len, IP_PROTOCOL_ICMP);
                HAL_SendIPPacket(if_index, output, ip_len, src_mac);
            }
        }
    }
    return 0;
}
