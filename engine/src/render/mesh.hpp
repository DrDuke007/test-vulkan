#pragma once
#include "base/types.hpp"
#include "base/vector.hpp"

#include <string>

struct SubMesh
{
    u32 first_index;
    u32 first_vertex;
    u32 index_count;
    u32 vertex_count;
};

struct Mesh
{
    std::string name;
    Vec<u32> indices;
    Vec<float3> positions;
    Vec<SubMesh> submeshes;

    bool operator==(const Mesh &other) const = default;
};
