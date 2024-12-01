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

Mesh create_sphere(float radius, size_t latitudes, size_t longitudes) {
    Mesh mesh;

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

            mesh.position_faces.push_back({current, next, aboveNext, above});
        }
    }

    // Bottom cap (triangles)
    uint32_t bottomVertex = mesh.positions.size() - 1;
    uint32_t baseIndex    = bottomVertex - longitudes;
    for (uint32_t lon = 0; lon < longitudes; lon++) {
        uint32_t current = baseIndex + lon;
        uint32_t next    = baseIndex + (lon + 1) % longitudes;
        mesh.position_faces.push_back({current, next, bottomVertex});
    }

    return mesh;
}
} // namespace rr
