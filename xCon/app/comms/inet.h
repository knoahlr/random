#ifndef INET_H
#define INET_H

#include <stdint.h>

// Lightweight replacement for inet_addr (host order!)
// Returns IP as uint32_t, or 0xFFFFFFFF on error.
uint32_t inet_addr_simple(const char *cp);

#endif