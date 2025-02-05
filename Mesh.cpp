#include "Mesh.h"
#include <array>

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

Mesh& Mesh::scale(const glm::vec3& s) {
    for (auto& pos : positions) {
        pos *= s;
    }
    return *this;
}

Mesh& Mesh::triangulate() {
    // Create temporary vectors to store the new triangulated faces
    std::vector<Face> new_position_faces;
    std::vector<Face> new_normal_faces;
    std::vector<Face> new_uv_faces;

    // Reserve space to avoid reallocations (worst case: all quads becoming 2 triangles)
    new_position_faces.reserve(position_faces.size() * 2);
    if (!normal_faces.empty())
        new_normal_faces.reserve(normal_faces.size() * 2);
    if (!uv_faces.empty())
        new_uv_faces.reserve(uv_faces.size() * 2);

    // Process each face
    for (size_t face_idx = 0; face_idx < position_faces.size(); ++face_idx) {
        const Face& pos_face = position_faces[face_idx];

        // If it's already a triangle, just copy it
        if (pos_face.size() == 3) {
            new_position_faces.push_back(pos_face);
            if (!normal_faces.empty())
                new_normal_faces.push_back(normal_faces[face_idx]);
            if (!uv_faces.empty())
                new_uv_faces.push_back(uv_faces[face_idx]);
            continue;
        }

        // For non-triangular faces, create a fan triangulation
        for (size_t i = 1; i < pos_face.size() - 1; ++i) {
            // Create new triangle face for positions
            Face new_pos_tri(3);
            new_pos_tri[0] = pos_face[0];     // center vertex
            new_pos_tri[1] = pos_face[i];     // current vertex
            new_pos_tri[2] = pos_face[i + 1]; // next vertex
            new_position_faces.push_back(new_pos_tri);

            // If we have normal topology, create corresponding triangle
            if (!normal_faces.empty()) {
                const Face& norm_face = normal_faces[face_idx];
                Face        new_norm_tri(3);
                new_norm_tri[0] = norm_face[0];
                new_norm_tri[1] = norm_face[i];
                new_norm_tri[2] = norm_face[i + 1];
                new_normal_faces.push_back(new_norm_tri);
            }

            // If we have UV topology, create corresponding triangle
            if (!uv_faces.empty()) {
                const Face& uv_face = uv_faces[face_idx];
                Face        new_uv_tri(3);
                new_uv_tri[0] = uv_face[0];
                new_uv_tri[1] = uv_face[i];
                new_uv_tri[2] = uv_face[i + 1];
                new_uv_faces.push_back(new_uv_tri);
            }
        }
    }

    // Replace the old faces with the triangulated ones
    position_faces = std::move(new_position_faces);
    if (!normal_faces.empty())
        normal_faces = std::move(new_normal_faces);
    if (!uv_faces.empty())
        uv_faces = std::move(new_uv_faces);

    return *this;
}

void set_flat_normals(Mesh& mesh) {
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
        mesh.normal_faces[f] = Mesh::Face(mesh.position_faces[f].size(), f);
    }
}

void set_smooth_normals(Mesh& mesh) {
    mesh.normals.assign(mesh.num_vertices(), glm::vec3(0));
    mesh.normal_faces.resize(mesh.num_faces());

    for (size_t f = 0; f < mesh.num_faces(); ++f) {
        const auto& face = mesh.position_faces[f];

        const glm::vec3& v0 = mesh.positions[face[0]];
        const glm::vec3& v1 = mesh.positions[face[1]];
        const glm::vec3& v2 = mesh.positions[face[2]];

        glm::vec3 edge1       = v1 - v0;
        glm::vec3 edge2       = v2 - v0;
        glm::vec3 face_normal = glm::normalize(glm::cross(edge1, edge2));

        for (uint32_t i : face) {
            mesh.normals[i] += face_normal;
        }

        mesh.normal_faces[f] = face;
    }

    for (size_t i = 0; i < mesh.normals.size(); ++i) {
        mesh.normals[i] = glm::normalize(mesh.normals[i]);
    }
}

bool is_triangulated(const Mesh& mesh) {
    for (const auto& face : mesh.position_faces) {
        if (face.size() != 3) {
            return false;
        }
    }
    return true;
}

} // namespace rr