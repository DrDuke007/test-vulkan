#include "types.h"

layout(set = 0, binding = 2) uniform sampler2D font_atlas;

layout(location = 0) in float2 i_uv;
layout(location = 1) in float4 i_color;
layout(location = 0) out float4 o_color;
void main() {
    o_color = texture( font_atlas, i_uv ) * i_color;
    o_color.rgb *= o_color.a;
}
