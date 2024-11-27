#include "RenderRex.h"

#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

// 1. load mesh
// 2. render mesh

int main() {

    std::string path = "C:\\Users\\janos\\Documents\\Projects\\RenderRex\\mammoth_simple.obj";

    std::vector<glm::vec3>          positions;
    std::vector<std::array<int, 3>> triangles;
    rr::load_mesh(path, positions, triangles);

    rr::register_mesh("mesh", positions, triangles);
    rr::show();

    return 0;
}
