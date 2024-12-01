#include "Primitives.h"

#include "glm/gtc/constants.hpp"

namespace rr {

Mesh create_box() {
    Mesh mesh;

    // Define the 8 vertices of the box
    // Front face vertices
    mesh.positions.emplace_back(-0.5f, -0.5f, 0.5f); // 0: front bottom left
    mesh.positions.emplace_back(0.5f, -0.5f, 0.5f);  // 1: front bottom right
    mesh.positions.emplace_back(0.5f, 0.5f, 0.5f);   // 2: front top right
    mesh.positions.emplace_back(-0.5f, 0.5f, 0.5f);  // 3: front top left

    // Back face vertices
    mesh.positions.emplace_back(-0.5f, -0.5f, -0.5f); // 4: back bottom left
    mesh.positions.emplace_back(0.5f, -0.5f, -0.5f);  // 5: back bottom right
    mesh.positions.emplace_back(0.5f, 0.5f, -0.5f);   // 6: back top right
    mesh.positions.emplace_back(-0.5f, 0.5f, -0.5f);  // 7: back top left

    // Define the 6 faces as quads (4 vertices each)
    // Front face (CCW)
    mesh.position_faces.push_back({0, 1, 2, 3});

    // Back face (CCW)
    mesh.position_faces.push_back({5, 4, 7, 6});

    // Top face (CCW)
    mesh.position_faces.push_back({3, 2, 6, 7});

    // Bottom face (CCW)
    mesh.position_faces.push_back({4, 5, 1, 0});

    // Right face (CCW)
    mesh.position_faces.push_back({1, 5, 6, 2});

    // Left face (CCW)
    mesh.position_faces.push_back({4, 0, 3, 7});

    return mesh;
}

Mesh create_sphere(size_t latitudes, size_t longitudes) {
    Mesh mesh;

    const float radius = 0.5f;

    // Generate vertices
    // Add top vertex
    mesh.positions.push_back(glm::vec3(0.0f, radius, 0.0f));

    // Generate vertices for each latitude ring
    for (uint32_t lat = 1; lat < latitudes; lat++) {
        float phi = glm::pi<float>() * float(lat) / float(latitudes);
        float y   = radius * cos(phi);
        float r   = radius * sin(phi);

        for (uint32_t lon = 0; lon < longitudes; lon++) {
            float theta = 2.0f * glm::pi<float>() * float(lon) / float(longitudes);
            float x     = r * sin(theta);
            float z     = r * cos(theta);
            mesh.positions.push_back(glm::vec3(x, y, z));
        }
    }

    // Add bottom vertex
    mesh.positions.push_back(glm::vec3(0.0f, -radius, 0.0f));

    // Generate faces
    // Top cap (triangles)
    for (uint32_t lon = 0; lon < longitudes; lon++) {
        uint32_t current = lon + 1;
        uint32_t next    = (lon + 1) % longitudes + 1;
        mesh.position_faces.push_back({0, current, next});
    }

    // Body (quads)
    for (uint32_t lat = 0; lat < latitudes - 2; lat++) {
        int baseIndex = 1 + lat * longitudes;
        for (uint32_t lon = 0; lon < longitudes; lon++) {
            uint32_t current   = baseIndex + lon;
            uint32_t next      = baseIndex + (lon + 1) % longitudes;
            uint32_t above     = current + longitudes;
            uint32_t aboveNext = next + longitudes;

            mesh.position_faces.push_back({above, aboveNext, next, current});
        }
    }

    // Bottom cap (triangles)
    uint32_t bottomVertex = mesh.positions.size() - 1;
    uint32_t baseIndex    = bottomVertex - longitudes;
    for (uint32_t lon = 0; lon < longitudes; lon++) {
        uint32_t current = baseIndex + lon;
        uint32_t next    = baseIndex + (lon + 1) % longitudes;
        mesh.position_faces.push_back({next, current, bottomVertex});
    }

    return mesh;
}

Mesh create_cylinder(size_t segments) {
    Mesh mesh;

    const float radius = 0.5f;
    const float height = 0.5f;

    // Generate vertices and normals for top and bottom circles
    for (size_t i = 0; i < segments; ++i) {
        float theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
        float x     = radius * std::cos(theta);
        float z     = radius * std::sin(theta);

        // Normal for the side (pointing outward from the center)
        glm::vec3 side_normal = glm::normalize(glm::vec3(x, 0.0f, z));

        // Bottom circle vertices
        mesh.positions.emplace_back(x, -height, z);
        mesh.normals.push_back(side_normal);

        // Top circle vertices
        mesh.positions.emplace_back(x, height, z);
        mesh.normals.push_back(side_normal);
    }

    // Generate faces
    // Side faces (quads)
    for (size_t i = 0; i < segments; ++i) {
        uint32_t current_bottom = i * 2;
        uint32_t current_top    = current_bottom + 1;
        uint32_t next_bottom    = ((i + 1) % segments) * 2;
        uint32_t next_top       = next_bottom + 1;

        mesh.position_faces.push_back({current_top, next_top, next_bottom, current_bottom});
        mesh.normal_faces.push_back({current_top, next_top, next_bottom, current_bottom});
    }

    // Bottom face (single polygon)
    rr::SmallVector<uint32_t, 4> bottom_face;
    rr::SmallVector<uint32_t, 4> bottom_normal_indices;
    uint32_t                     bottom_normal = mesh.normals.size();
    mesh.normals.push_back(glm::vec3(0.0f, -1.0f, 0.0f));

    for (size_t i = 0; i < segments; ++i) {
        bottom_face.push_back(i * 2); // Bottom vertices
        bottom_normal_indices.push_back(bottom_normal);
    }
    mesh.position_faces.push_back(bottom_face);
    mesh.normal_faces.push_back(bottom_normal_indices);

    // Top face (single polygon)
    rr::SmallVector<uint32_t, 4> top_face;
    rr::SmallVector<uint32_t, 4> top_normal_indices;
    uint32_t                     top_normal = mesh.normals.size();
    mesh.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));

    for (size_t i = 0; i < segments; ++i) {
        top_face.push_back(((segments - i - 1) * 2) + 1); // Top vertices in reverse order for CCW
        top_normal_indices.push_back(top_normal);
    }
    mesh.position_faces.push_back(top_face);
    mesh.normal_faces.push_back(top_normal_indices);

    return mesh;
}

Mesh create_cone(size_t segments) {
    Mesh        mesh;
    const float height = 0.5f;
    const float radius = 0.5f;

    // Generate vertices and normals for base circle
    for (size_t i = 0; i < segments; ++i) {
        float theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
        float x     = radius * std::cos(theta);
        float z     = radius * std::sin(theta);

        // Calculate smooth normal for the side
        glm::vec3 radial_dir  = glm::normalize(glm::vec3(x, 0.0f, z));
        float     angle       = std::atan2(height, radius);
        glm::vec3 side_normal = glm::normalize(glm::vec3(radial_dir.x, std::tan(angle), radial_dir.z));

        mesh.positions.emplace_back(x, -height, z);
        mesh.normals.push_back(side_normal);
    }

    // Add apex vertex and normal
    uint32_t apex = mesh.positions.size();
    mesh.positions.emplace_back(0.0f, height, 0.0f);

    // Add apex normals (one for each segment for smooth shading)
    for (size_t i = 0; i < segments; ++i) {
        float theta = 2.0f * glm::pi<float>() * float(i) / float(segments);
        float x     = std::cos(theta);
        float z     = std::sin(theta);

        float     angle       = std::atan2(height, radius);
        glm::vec3 apex_normal = glm::normalize(glm::vec3(x, std::tan(angle), z));
        mesh.normals.push_back(apex_normal);
    }

    // Generate faces
    // Side faces (triangles)
    for (size_t i = 0; i < segments; ++i) {
        uint32_t current     = i;
        uint32_t next        = (i + 1) % segments;
        uint32_t apex_normal = segments + i;

        mesh.position_faces.push_back({next, current, apex});
        mesh.normal_faces.push_back({next, current, apex_normal});
    }

    // Base face (single polygon)
    rr::SmallVector<uint32_t, 4> base_face;
    rr::SmallVector<uint32_t, 4> base_normal_indices;
    uint32_t                     base_normal = mesh.normals.size();
    mesh.normals.push_back(glm::vec3(0.0f, -1.0f, 0.0f));

    for (size_t i = 0; i < segments; ++i) {
        base_face.push_back(i);
        base_normal_indices.push_back(base_normal);
    }
    mesh.position_faces.push_back(base_face);
    mesh.normal_faces.push_back(base_normal_indices);

    return mesh;
}

} // namespace rr
