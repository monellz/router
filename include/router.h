#ifndef __ROUTER_H
#define __ROUTER_H
#include <stdint.h>
typedef struct {
    uint32_t addr;
    uint32_t len;
    uint32_t if_index;
    uint32_t nexthop;
    uint32_t metric;
    uint64_t timestamp;
} RoutingTableEntry;
#endif