#include "router_hal.h"

// configure this to match the output of `ip a`
const char *interfaces[N_IFACE_ON_BOARD] = {
    "pc1r-r",
    "pc2r-r",
    "eth2",
    "eth3",
};
