#extension GL_ARB_shader_draw_parameters : require

#include "types.h"

struct ImGuiVertex
{
    float2 position;
    float2 uv;
    float4 color;
};

struct DrawData
{
    float2 scale;
    float2 translation;
    uint vertex_offset;
};

layout(set = 0, binding = 0) buffer readonly Vertices {
    ImGuiVertex vertices[];
};

layout(set = 0, binding = 1) buffer readonly DrawDatas {
    DrawData draw_datas[];
};

layout(location = 0) out float2 o_uv;
layout(location = 1) out float4 o_color;
void main()
{
    DrawData draw = draw_datas[gl_DrawIDARB];
    ImGuiVertex vertex = vertices[gl_VertexIndex + draw.vertex_offset];

    gl_Position = float4( vertex.position * draw.scale + draw.translation, 0.0, 1.0 );
    o_uv = vertex.uv;
    o_color = vertex.color;
}
