#include "RenderRex.h"

#include "glm/glm.hpp"

#include <array>
#include <string>
#include <vector>

rr::Mesh join(const rr::Mesh& a, const rr::Mesh& b) {
    rr::Mesh result;

    // Concatenate positions
    result.positions.insert(result.positions.end(), a.positions.begin(), a.positions.end());
    result.positions.insert(result.positions.end(), b.positions.begin(), b.positions.end());

    // Copy faces from first mesh directly
    result.position_faces.insert(result.position_faces.end(), a.position_faces.begin(), a.position_faces.end());

    // Offset and copy faces from second mesh
    const size_t vertex_offset = a.positions.size();
    auto         offset_faces  = b.position_faces;
    for (rr::Mesh::Face& face : offset_faces) {
        for (uint32_t& index : face) {
            index += vertex_offset;
        }
    }
    result.position_faces.insert(result.position_faces.end(), offset_faces.begin(), offset_faces.end());
    return result;
}

rr::Mesh create_arrow() {
    auto cylinder = rr::create_cylinder().scale({0.3f, 2.f, 0.3f});
    auto cone     = rr::create_cone().translate({0.f, 1.5f, 0.f});
    return join(cylinder, cone);
}

int main() {
    // std::vector<glm::vec3>          positions;
    // std::vector<std::array<int, 3>> triangles;

    // TODO: with the current system this generates too much geometry
    //std::string path  = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    //rr::Mesh    mesh  = rr::load_mesh(path);
    //rr::set_flat_normals(mesh);
    //rr::RenderMesh* rm = rr::register_mesh("mesh", mesh);
    //rm->add_face_attribute("face normals", mesh.normals);
    //rr::show();

    rr::Mesh mesh = rr::create_sphere();
    rr::set_flat_normals(mesh);
    rr::RenderMesh* rm = rr::register_mesh("mesh", mesh);
    rm->add_face_attribute("face normals", mesh.normals);

    // rr::register_mesh("sphere", rr::create_sphere());
    // rr::register_mesh("arrow", create_arrow());
    // rr::register_mesh("box", rr::create_box().translate({-1.5f, 0.f, -0.5f}));
    // rr::register_mesh("cylinder", rr::create_cylinder().translate({1.5f, 0.f, -0.5f}));
    // rr::register_mesh("cone", rr::create_cone().translate({0.f, 0.f, 1.5f,}));

    rr::show();

    return 0;
}
