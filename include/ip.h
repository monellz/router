#ifndef __IP_H
#define __IP_H
#include <stdint.h>
#include <stdlib.h>

#define IP_VERSION_HEADER_LENGTH    0
#define IP_TYPE_OF_SERVICE          1 
#define IP_TOTAL_LENGTH_0           2
#define IP_TOTAL_LENGTH_1           3 
#define IP_IDENTIFIER_0             4
#define IP_IDENTIFIER_1             5
#define IP_FLAGS_FRAMENTED_OFFSET_0 6
#define IP_FLAGS_FRAMENTED_OFFSET_1 7
#define IP_TTL                      8
#define IP_PROTOCOL                 9
#define IP_HEADER_CHECKSUM_0        10
#define IP_HEADER_CHECKSUM_1        11
#define IP_SRC_ADDR_0               12
#define IP_SRC_ADDR_1               13
#define IP_SRC_ADDR_2               14
#define IP_SRC_ADDR_3               15
#define IP_DST_ADDR_0               16
#define IP_DST_ADDR_1               17
#define IP_DST_ADDR_2               18
#define IP_DST_ADDR_3               19


#define IP_DEFAULT_VERSION_HEADER_LENGTH    0x45
#define IP_DEFAULT_HEADER_LENGTH            20
#define IP_DEFAULT_TTL                      64

#define IP_PROTOCOL_UDP                     17


#endif