#ifndef __ROUTE_TABLE_H
#define __ROUTE_TABLE_H
#include <stdint.h>
#include <stdio.h>

#include "rip.h"

#define ROUTE_MAX  32768
#define ROUTE_MOD  3277

//addr is big end
#define IPFORMAT(addr) (addr & 0xff), ((addr >> 8) & 0xff), ((addr >> 16) & 0xff), ((addr >> 24) & 0xff)
//table entry
struct Entry {
    uint32_t addr;
    uint32_t mask_len;
    uint32_t if_index;
    uint32_t nexthop;
    uint32_t metric;
    uint32_t next; //next entry
};


uint32_t mask_right(uint32_t mask_len);
uint32_t mask_left(uint32_t mask_len);
uint32_t mask_len_from_mask_right(uint32_t mask);

void route_insert(uint32_t dst_addr, uint32_t mask_len, uint32_t if_index, uint32_t nexthop, uint32_t metric);
void route_delete(uint32_t dst_addr, uint32_t mask_len);
bool route_query(uint32_t addr, uint32_t *nexthop, uint32_t *if_index, uint32_t *metric);


void route_print(uint32_t addr, uint32_t mask_len, uint32_t nexthop, uint32_t if_index, uint32_t metric, const char *info);
void route_print_all(const char *info);

void route_fill_rip_packet(RipPacket *packet);

#endif