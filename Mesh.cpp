#include "Mesh.h"

namespace rr {

Mesh::Mesh(const std::vector<glm::vec3>& pts, const std::vector<std::array<uint32_t, 3>>& triangles) : positions(pts) {
    position_faces.resize(triangles.size());
    for (size_t f = 0; f < triangles.size(); ++f) {
        position_faces[f] = {triangles[f][0], triangles[f][1], triangles[f][2]};
    }
}

Mesh& Mesh::translate(const glm::vec3& p) {
    for (auto& pos : positions) {
        pos += p;
    }
    return *this;
}

void create_flat_normals(Mesh& mesh) {
    mesh.normals.resize(mesh.num_faces());
    mesh.normal_faces.resize(mesh.num_faces());

    // Calculate one normal per face
    for (size_t f = 0; f < mesh.num_faces(); ++f) {
        const auto& face = mesh.position_faces[f];

        const glm::vec3& v0 = mesh.positions[face[0]];
        const glm::vec3& v1 = mesh.positions[face[1]];
        const glm::vec3& v2 = mesh.positions[face[2]];

        glm::vec3 edge1  = v1 - v0;
        glm::vec3 edge2  = v2 - v0;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        mesh.normals[f]      = normal;
        mesh.normal_faces[f] = Mesh::Face(f, mesh.position_faces[f].size());
    }
}

void create_smooth_normals(Mesh& mesh) {
    mesh.normals.resize(mesh.num_vertices());
    mesh.normal_faces.resize(mesh.num_faces());

    for (size_t f = 0; f < mesh.num_faces(); ++f) {
        const auto& face = mesh.position_faces[f];
        assert(face.size() == 3);

        const glm::vec3& v0 = mesh.positions[face[0]];
        const glm::vec3& v1 = mesh.positions[face[1]];
        const glm::vec3& v2 = mesh.positions[face[2]];

        glm::vec3 edge1       = v1 - v0;
        glm::vec3 edge2       = v2 - v0;
        glm::vec3 face_normal = glm::normalize(glm::cross(edge1, edge2));

        for (size_t i = 0; i < face.size(); ++i) {
            mesh.normals[face[i]] += face_normal;
        }
    }

    for (size_t i = 0; i < mesh.normals.size(); ++i) {
        mesh.normals[i] = glm::normalize(mesh.normals[i]);
    }
}

} // namespace rr