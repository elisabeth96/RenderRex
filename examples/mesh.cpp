#include "RenderRex.h"
#include "Utils.h"

int main() {
    std::string path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    rr::Mesh mesh = rr::load_mesh(path);

    rr::VisualMesh* visual_mesh = rr::make_visual("mammoth", mesh);

    rr::show();

    return 0;
}
