#include "rdRaster.h"

void rdRaster_Startup()
{
    rdRaster_aOneDivXLUT[0] = 3.4e38;
    rdRaster_aOneDivXQuantLUT[0] = 0x7FFFFFFF;

    for (int i = 1; i < 2048; i++)
    {
        rdRaster_aOneDivXLUT[i] = 1.0 / (flex_d_t)i;
        rdRaster_aOneDivXQuantLUT[i] = (int)(1.0 / (flex_d_t)i * 65536.0);
    }

    // j=0 is a sentinel (1/0 = ~infinity); multiplying it overflows on N64.
    // Mirror the same sentinel pattern used for the primary LUT.
    rdRaster_aOtherLUT[0] = 3.4e38f;
    for (int j = 1; j < 16; j++)
    {
        rdRaster_aOtherLUT[j] = rdRaster_aOneDivXLUT[j] * rdRaster_fixedScale;
    }
}
