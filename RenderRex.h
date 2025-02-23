// contains all the unser interface funtions for the renderrex library
#pragma once

#include "Drawable.h"
#include "InstancedMesh.h"
#include "Mesh.h"
#include "Primitives.h"
#include "VisualMesh.h"

#include "glm/glm.hpp"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace rr {

// function to show all the meshes that are saved wherever (tbd)
// for now, just creates a window and displays a nice color
void show();

VisualMesh* make_visual(std::string name, std::vector<glm::vec3>& positions,
                        std::vector<std::array<uint32_t, 3>>& triangles);

VisualMesh* make_visual(std::string name, const Mesh& mesh);

VisualPointCloud* make_visual(std::string name, const std::vector<glm::vec3>& pos);

VisualLineNetwork* make_visual(std::string name, const std::vector<glm::vec3>& pos,
                               const std::vector<std::pair<int, int>>& lines);

InstancedMesh* make_instanced(std::string name, const Mesh& mesh, size_t num_instances);

void set_user_callback(std::function<void()> callback);

} // namespace rr
