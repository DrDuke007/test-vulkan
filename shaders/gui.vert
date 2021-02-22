#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    uint color;
    uint pad00;
};

layout(set = 0, binding = 0) buffer readonly Vertices {
    ImGuiVertex vertices[];
};

layout(set = 0, binding = 1) buffer readonly Options {
    float2 scale;
    float2 translation;
};

layout(location = 0) out float2 o_uv;
layout(location = 1) out float4 o_color;
void main()
{
    ImGuiVertex vertex = vertices[gl_VertexIndex];

    gl_Position = float4( vertex.position * scale + translation, 0.0, 1.0 );
    o_uv = vertex.uv;
    o_color = unpackUnorm4x8(vertex.color);
}
