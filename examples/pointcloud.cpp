#include "RenderRex.h"
#include "Utils.h"

int main() {
    rr::Mesh spot = rr::load_mesh(std::string(RESOURCE_DIR) + "/spot.obj");

    rr::VisualPointCloud* vpc = rr::make_visual("spot_points", spot.positions);
    
    rr::show();

    return 0;
}
