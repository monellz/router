#include "route_table.h"


Entry elem[ROUTE_MAX];
uint32_t last[33][ROUTE_MOD] = {{0}}, elem_last = 0;
//0 is invalid
uint32_t valid_mask_len = 0; //just for speeding up

uint32_t mask_right(uint32_t mask_len) {
    return ((uint64_t)1 << mask_len) - 1;
}

uint32_t mask_left(uint32_t mask_len) {
    return (((uint64_t)0xffffffff0000000 >> mask_len));
}

uint32_t mask_len_from_mask_right(uint32_t mask) {
    //0000...00011111...111
    uint32_t len = 0;
    if ((mask >> 16) > 0) { len += 16; mask >>= 16; }
    if ((mask >> 8) > 0) { len += 8; mask >>= 8; }
    if ((mask >> 4) > 0) { len += 4; mask >>= 4; }
    if ((mask >> 2) > 0) { len += 2; mask >>= 2; }
    if ((mask >> 1) > 0) { len += 1; mask >>= 1; }
    return mask == 1? len + 1: len;
}


void route_insert(uint32_t dst_addr, uint32_t mask_len, uint32_t if_index, uint32_t nexthop, uint32_t metric) {
    uint32_t mask = mask_right(mask_len);
    uint32_t key = (dst_addr & mask) % ROUTE_MOD;
    valid_mask_len = mask_len > valid_mask_len? mask_len: valid_mask_len;
    bool found = false;
    for (uint32_t i = last[mask_len][key]; i != 0; i = elem[i].next) {
        if (elem[i].addr == dst_addr && elem[i].mask_len == mask_len) {
            //update
            elem[i] = Entry {dst_addr, mask_len, if_index, nexthop, metric, elem[i].next};
            found = true;
        }
    }    
    if (!found) {   
        elem[++elem_last] = Entry {dst_addr, mask_len, if_index, nexthop, metric, last[mask_len][key]};
        last[mask_len][key] = elem_last;
    }
}

void route_delete(uint32_t dst_addr, uint32_t mask_len) {
    //NOT SUPPORT DELETE NOW
    /*
    uint32_t mask = mask_right(mask_len);
    uint32_t key = (dst_addr & mask) % ROUTE_MOD;
    for (uint32_t i = last[mask_len][key], p = 0; i != 0; p = i, i = elem[i].next) {
        if (elem[i].addr == dst_addr) {
            //del
            if (elem[i].next == 0) {
                last[mask_len][key] = p;
            } else {
                elem[i] = elem[elem[i].next];
            }
        }
    }
    */
}

bool route_query(uint32_t addr, uint32_t *nexthop, uint32_t *if_index, uint32_t *metric) {
    #pragma unroll
    //for (uint32_t i = 0; i < 32; ++i) {
    for (uint32_t mask_len = valid_mask_len; mask_len > 0; mask_len--) {
        uint32_t mask = mask_right(mask_len);
        for (uint32_t idx = last[mask_len][(addr & mask) % ROUTE_MOD]; idx != 0; idx = elem[idx].next) {
            //if ((addr & mask) == (elem[idx].addr & mask) && ((addr & 0x1) == (elem[idx].addr & 0x1))) {
            if ((addr & mask) == (elem[idx].addr & mask)) {
                //found
                *nexthop = elem[idx].nexthop;
                *if_index = elem[idx].if_index;
                *metric = elem[idx].metric;
                return true;
            }
        }
    }
    return false;    
}

void route_fill_rip_packet(RipPacket *packet, uint32_t num_offset, uint32_t if_index) {
    //response
    uint32_t entry_num = 0; 
    packet->command = RIP_CMD_RESPONSE;
    for (uint32_t idx = 1 + num_offset; idx <= elem_last && idx <= RIP_MAX_ENTRY + num_offset; ++idx) {
        //assume there is not duplicate entry
        //it needs support of DELETE
        packet->entries[entry_num].addr = elem[idx].addr;
        packet->entries[entry_num].mask = mask_right(elem[idx].mask_len);
        packet->entries[entry_num].metric = if_index == elem[idx].if_index? RIP_METRIC_INFINITY: elem[idx].metric;
        packet->entries[entry_num].nexthop = elem[idx].nexthop;
        entry_num++;
    }
    packet->numEntries = entry_num;

    /*
    if (entry_num > RIP_MAX_ENTRY) {
        printf("Warning! table entry num is larger than RIP_MAX_ENTRY(%d)\n", RIP_MAX_ENTRY);
    }
    */
}


void route_print(uint32_t addr, uint32_t mask_len, uint32_t nexthop, uint32_t if_index, uint32_t metric, const char *info) {
    printf("Route[%s]: addr=%d.%d.%d.%d/%d, nexthop=%d.%d.%d.%d, if_index=%d, metric=%d\n", info, IPFORMAT(addr), mask_len, IPFORMAT(nexthop), if_index, metric);
}

void route_print_all(const char *info) {
    printf("Route num is %d\n", elem_last);
    if (elem_last <= RIP_MAX_ENTRY) {
        for (uint32_t idx = 1; idx <= elem_last; ++idx) {
            route_print(elem[idx].addr, elem[idx].mask_len, elem[idx].nexthop, elem[idx].if_index, elem[idx].metric, info);
        }
    }
}
