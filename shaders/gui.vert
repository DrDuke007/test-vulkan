#extension GL_ARB_shader_draw_parameters : require
#extension GL_EXT_buffer_reference : require

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

layout(buffer_reference, std430, buffer_reference_align = 16) buffer VerticesType {
    ImGuiVertex vertices[];
};

layout(set = 0, binding = 1) uniform Options {
    float2 scale;
    float2 translation;
    VerticesType vertices_ptr;
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
