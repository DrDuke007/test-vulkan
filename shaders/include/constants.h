#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "types.h"

const float PI = 3.1415926535897932384626433832795;
const u32 u32_invalid = ~0u;
const int LOD0 = 0;
const float TO_RADIANS = PI / 180.0;
const float TO_DEGRES = 180 / PI;

#define dbg(x) o_color = float4(x, 1.0); return;

#endif
