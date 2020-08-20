#include "types.h"

layout(set = 1, binding = 0) uniform sampler2D hdr_buffer;

layout(set = 1, binding = 1) uniform DO
{
    uint selected;
    float exposure;
} debug;


layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

void main()
{
    float3 ldr = texture(hdr_buffer, inUV).rgb;
    float3 hdr = float3(0.0);

    if (debug.selected == 1)
    {
        hdr = vec3(1.0) - exp(-ldr * debug.exposure);
    }
    else if (debug.selected == 2)
    {
        hdr = clamp(ldr, 0.0, 1.0);
    }
    else
    {
        hdr = ldr / (ldr + 1.0);
    }

    // to srgb
    hdr = pow(hdr, float3(1.0 / 2.2));

    outColor = vec4(hdr, 1.0);
}
