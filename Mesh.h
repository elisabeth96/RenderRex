#pragma once

#include <vector>
#include "SmallVector.h"

#include "glm/glm.hpp"

namespace rr {

struct Mesh {
    Mesh() = default;

    explicit Mesh(const std::vector<glm::vec3>& positions, const std::vector<std::array<uint32_t, 3>>& triangles);

    using Face = SmallVector<uint32_t, 4>;

    std::vector<glm::vec3> positions;
    std::vector<Face> position_faces;

    // Optionally, this mesh struct also supports storing
    // normals and uvs with proper normal and uv topology.

    std::vector<glm::vec3> normals;
    std::vector<Face> normal_faces;

    std::vector<glm::vec2> uvs;
    std::vector<Face> uv_faces;

    size_t num_faces() const {
        return position_faces.size();
    }

    size_t num_vertices() const {
        return positions.size();
    }

    Mesh& translate(const glm::vec3& p);
    Mesh& scale(const glm::vec3& s);
    Mesh& triangulate();
};

void set_flat_normals(Mesh& mesh);
void set_smooth_normals(Mesh& mesh);
bool is_triangulated(const Mesh& mesh);

}
