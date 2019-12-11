#ifndef __ICMP_H
#define __ICMP_H

#define ICMP_TYPE               0
#define ICMP_CODE               1
#define ICMP_TOTAL_CHECKSUM_0   2
#define ICMP_TOTAL_CHECKSUM_1   3


//only for error packet
#define ICMP_NO_USE_0           4
#define ICMP_NO_USE_1           5 
#define ICMP_NO_USE_2           6 
#define ICMP_NO_USE_3           7 

#define ICMP_IP_HEAD            8

#define ICMP_TYPE_TIME_EXCEEDED     11
#define ICMP_TYPE_DEST_UNREACHABLE  3

#define ICMP_CODE_FRAGMENT_REASSEMBLY_TIME_EXCEEDED     1
#define ICMP_CODE_NETWORK_UNREACHABLE                   0


#endif