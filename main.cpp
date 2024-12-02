#include "MeshIO.h"
#include "RenderRex.h"

#include <fstream>
#include <sstream>
#include <string>

extern const char spot_data[];

std::vector<glm::vec3> compute_face_normals(const rr::Mesh& mesh) {
    std::vector<glm::vec3> normals(mesh.num_faces());
    for (size_t f = 0; f < mesh.num_faces(); ++f) {
        const auto&      face   = mesh.position_faces[f];
        const glm::vec3& v0     = mesh.positions[face[0]];
        const glm::vec3& v1     = mesh.positions[face[1]];
        const glm::vec3& v2     = mesh.positions[face[2]];
        glm::vec3        edge1  = v1 - v0;
        glm::vec3        edge2  = v2 - v0;
        glm::vec3        normal = glm::normalize(glm::cross(edge1, edge2));
        normals[f]              = normal;
    }
    return normals;
}

rr::FaceVectorProperty* fa = nullptr;

void callback() {
    static float time  = 0.0f;
    const float  speed = 0.02f;
    time += speed;

    float scale = 1.0f + std::sin(time) * 0.5f; // Oscillates between 0.5 and 1.5
    fa->set_length(scale);
    fa->set_radius(scale);
    float     t     = (std::sin(time) + 1.0f) * 0.5f;
    glm::vec3 color = glm::vec3(1.0f - t, t, 0.0f);
    fa->set_color(color);
}

int main() {
    // TODO: with the current system this generates too much geometry
    // std::string path  = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    // rr::Mesh spot = rr::load_mesh(path);

    std::stringstream mesh_data(spot_data);
    rr::Mesh          spot = rr::load_mesh(mesh_data);
    rr::VisualMesh*   vm   = rr::make_visual("spot", spot);
    fa                     = vm->add_face_attribute("face normals", compute_face_normals(spot));
    rr::set_user_callback(callback);
    rr::show();

    return 0;
}
