#include "RenderRex.h"

#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

int main() {
    //std::string path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    //std::vector<glm::vec3>          positions;
    //std::vector<std::array<int, 3>> triangles;
    //rr::load_mesh(path, positions, triangles);

    rr::register_mesh("mesh", rr::create_box());

    rr::show();

    return 0;
}
