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

void load_mesh(std::string path, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles) {
    positions.clear();
    triangles.clear();

    std::ifstream input_file(path);
    if (!input_file.is_open()) {
        std::cerr << "Could not open file " << path << std::endl;
        return;
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
            std::array<int, 3> triangle{};
            iss >> triangle[0] >> triangle[1] >> triangle[2];
            triangle[0] -= 1;
            triangle[1] -= 1;
            triangle[2] -= 1;
            triangles.push_back(triangle);
        }
    }
}

void register_mesh(std::string name, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles) {
    Renderer& renderer = Renderer::get();
    renderer.register_mesh(name, positions, triangles);
}

} // namespace rr
