#pragma once

// OGG page CRC32 checksum (polynomial 0x04c11db7, from Tremor/lowmem).
// Used by Bitoggstream::FlushPage to compute the checksum field of each OGG page.

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    uint32_t Checksum(unsigned char* data, int bytes);

#ifdef __cplusplus
}
#endif // __cplusplus
