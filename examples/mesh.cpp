#include "RenderRex.h"
#include "Utils.h"

int main() {
    std::string path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    rr::Mesh mesh = rr::load_mesh(path);

    auto sphere = rr::create_sphere(10, 10).scale(0.1f);

    rr::VisualMesh* vm = rr::make_visual("mammoth", mesh);

    rr::VisualMesh* vs = rr::make_visual("sphere", sphere);
    vs->set_hide_mesh(true);

    rr::show();

    return 0;
}
