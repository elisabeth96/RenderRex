// contains all the unser interface funtions for the renderrex library
#pragma once

#include "Mesh.h"
#include "Drawable.h"
#include "Primitives.h"

#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

namespace rr {

// function to show all the meshes that are saved wherever (tbd)
// for now, just creates a window and displays a nice color
void show();

void load_mesh(std::string path, std::vector<glm::vec3>& positions, std::vector<std::array<uint32_t, 3>>& triangles);

Mesh load_mesh(std::string path);

RenderMesh* register_mesh(std::string name, std::vector<glm::vec3>& positions, std::vector<std::array<uint32_t, 3>>& triangles);

RenderMesh* register_mesh(std::string name, const Mesh& mesh);

} // namespace rr
