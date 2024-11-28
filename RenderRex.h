// contains all the unser interface funtions for the renderrex library
#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

namespace rr {

// function to show all the meshes that are saved wherever (tbd)
// for now, just creates a window and displays a nice color
void show();

void load_mesh(std::string path, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles);

void register_mesh(std::string name, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles);

} // namespace rr
