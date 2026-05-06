#ifndef _N64_ENDIAN_H
#define _N64_ENDIAN_H

#include <stdint.h>

/**
 * N64 is Big-Endian (VR4300).
 * OpenJKDF2 PC assets are Little-Endian.
 * These macros help swap bytes when reading binary assets.
 */

static inline uint16_t bswap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static inline uint32_t bswap32(uint32_t val) {
    return ((val << 24) & 0xff000000 ) |
           ((val <<  8) & 0x00ff0000 ) |
           ((val >>  8) & 0x0000ff00 ) |
           ((val >> 24) & 0x000000ff );
}

static inline float bswapf32(float val) {
    union {
        float f;
        uint32_t i;
    } u;
    u.f = val;
    u.i = bswap32(u.i);
    return u.f;
}

#ifdef TARGET_N64
    #define GU_SWAP16(x) bswap16(x)
    #define GU_SWAP32(x) bswap32(x)
    #define GU_SWAPF(x)  bswapf32(x)
#else
    #define GU_SWAP16(x) (x)
    #define GU_SWAP32(x) (x)
    #define GU_SWAPF(x)  (x)
#endif

#endif // _N64_ENDIAN_H
