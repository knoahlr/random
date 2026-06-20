#include "inet.h"
#include <stdio.h>

uint32_t inet_addr_simple(const char *cp) {
    unsigned int b1, b2, b3, b4;
    if (sscanf(cp, "%u.%u.%u.%u", &b1, &b2, &b3, &b4) == 4) {
        return ((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
    }
    return 0xFFFFFFFF;
}