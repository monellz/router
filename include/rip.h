#ifndef __RIP_H
#define __RIP_H
#include <stdint.h>
#define RIP_MAX_ENTRY 25

#define RIP_CMD_REQUEST   1
#define RIP_CMD_RESPONSE  2
#define RIP_AF_REQUEST    0
#define RIP_AF_RESPONSE   2
#define RIP_VERSION       2
#define RIP_TAG           0
#define RIP_ZERO          0

#define RIP_MULTICAST_ADDR 0x090000e0

#define RIP_METRIC_INFINITY 16

typedef struct {
    // all fields are big endian EXCEPT metric
    // we don't store 'family', as it is always 2(response) and 0(request)
    // we don't store 'tag', as it is always 0
    uint32_t addr;
    uint32_t mask;
    uint32_t nexthop;
    uint32_t metric; //small end
} RipEntry;

typedef struct {
    uint32_t numEntries;
    uint8_t command;
    // we don't store 'version', as it is always 2
    // we don't store 'zero', as it is always 0
    RipEntry entries[RIP_MAX_ENTRY];
} RipPacket;

#endif
