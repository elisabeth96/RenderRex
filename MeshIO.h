#pragma once

#include "glm/fwd.hpp"
#include <string_view>

namespace rr {

struct Mesh;

void load_mesh(std::string_view path, std::vector<glm::vec3>& positions,
               std::vector<std::array<uint32_t, 3>>& triangles);

Mesh load_mesh(std::string_view path);

Mesh load_mesh(std::istream& stream);


void save_obj(std::string_view path, const Mesh& mesh);

} // namespace rr
