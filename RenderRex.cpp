// contains all the unser interface funtions for the renderrex library

#include "RenderRex.h"
#include "Renderer.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace rr {

void show() {
    Renderer& renderer = Renderer::get();
    // continuously update window and react to user input
    while (!renderer.should_close()) {
        renderer.update_frame();
    }
}

Mesh load_mesh(std::string path) {
    Mesh  mesh;
    auto& positions = mesh.positions;
    auto& triangles = mesh.position_faces;

    std::ifstream input_file(path);
    if (!input_file.is_open()) {
        std::cerr << "Could not open file " << path << std::endl;
        return {};
    }

    std::string line;
    while (std::getline(input_file, line)) {
        std::istringstream iss(line);
        std::string        token;
        iss >> token;
        if (token == "v") {
            glm::vec3 position;
            iss >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (token == "f") {
            Mesh::Face triangle(3);
            iss >> triangle[0] >> triangle[1] >> triangle[2];
            triangle[0] -= 1;
            triangle[1] -= 1;
            triangle[2] -= 1;
            triangles.push_back(triangle);
        }
    }

    return mesh;
}

void load_mesh(std::string path, std::vector<glm::vec3>& positions, std::vector<std::array<uint32_t, 3>>& triangles) {
    Mesh mesh = load_mesh(path);
    if (!is_triangulated(mesh)) {
        mesh.triangulate();
    }
    positions = mesh.positions;
    triangles.resize(mesh.position_faces.size());
    for (size_t i = 0; i < mesh.position_faces.size(); ++i) {
        for (size_t j = 0; j < 3; ++j) {
            triangles[i][j] = mesh.position_faces[i][j];
        }
    }
}

RenderMesh* register_mesh(std::string name, const std::vector<glm::vec3>& positions,
                          const std::vector<std::array<uint32_t, 3>>& triangles) {
    Mesh mesh(positions, triangles);
    return register_mesh(name, mesh);
}

RenderMesh* register_mesh(std::string name, const Mesh& mesh) {
    Renderer& renderer = Renderer::get();
    if (mesh.normal_faces.empty()) {
        Mesh m(mesh);
        set_flat_normals(m);
        return renderer.register_mesh(name, m);
    } else {
        return renderer.register_mesh(name, mesh);
    }
}

} // namespace rr
