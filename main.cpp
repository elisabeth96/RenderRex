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
rr::VisualPointCloud* vpc = nullptr;

void callback2() {
    static float time  = 0.0f;
    const float  speed = 0.02f;
    time += speed;

    float scale = 1.0f + std::sin(time) * 0.5f; // Oscillates between 0.5 and 1.5
    vpc->set_radius(scale * 0.001f);
    float     t     = (std::sin(time) + 1.0f) * 0.5f;
    glm::vec3 color = glm::vec3(1.0f - t, t, 0.0f);
    vpc->set_color(color);
}

// UI

// Mesh
//  - face vector
//  - face color
//  - vertex vector
//
// Point cloud
//  - vertex vector
//  - vertex color
//  - point radius
//
// LineNetwork
//  - color
//  - thickness

/* int main() {
    // TODO: with the current system this generates too much geometry
    std::string path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    rr::Mesh    spot = rr::load_mesh(path);

    // std::stringstream mesh_data(spot_data);
    // rr::Mesh          spot = rr::load_mesh(mesh_data);
    rr::VisualMesh* vm = rr::make_visual("spot", spot);
    // fa                 = vm->add_face_attribute("face normals", compute_face_normals(spot));

    // rr::set_user_callback(callback);

    rr::show();

    return 0;
}*/

// point cloud

glm::vec3 hsvToRgb(float hue, float saturation, float value) {
    float     c = value * saturation;
    float     x = c * (1.f - std::abs(std::fmod(hue / 60.0f, 2.f) - 1.f));
    float     m = value - c;
    glm::vec3 rgb;

    if (hue < 60)
        rgb = {c, x, 0};
    else if (hue < 120)
        rgb = {x, c, 0};
    else if (hue < 180)
        rgb = {0, c, x};
    else if (hue < 240)
        rgb = {0, x, c};
    else if (hue < 300)
        rgb = {x, 0, c};
    else
        rgb = {c, 0, x};

    return rgb + glm::vec3(m);
}

glm::vec3 get_random_color() {
    static float hue = 42.0f;
    hue              = std::fmod(hue + 137.5f, 360.0f);
    return hsvToRgb(hue, 0.75f, 0.9f);
}

int main() {
    std::string            path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    rr::Mesh               mesh = rr::load_mesh(path);
    std::vector<glm::vec3> pcl  = mesh.positions;

    std::vector<glm::vec3> pcl2(pcl.begin(), pcl.begin() + 1000);
    std::vector<glm::vec3> pcl3(pcl.begin() + 1000, pcl.begin() + 2000);
    std::vector<glm::vec3> pcl4(pcl.begin() + 2000, pcl.begin() + 3000);
    std::vector<glm::vec3> pcl5(pcl.begin() + 3000, pcl.begin() + 4000);

    vpc                        = rr::make_visual("cloud", pcl2);
    rr::VisualPointCloud* vpc2 = rr::make_visual("cloud2", pcl3);
    rr::VisualPointCloud* vpc3 = rr::make_visual("cloud3", pcl4);
    rr::VisualPointCloud* vpc4 = rr::make_visual("cloud4", pcl5);
    rr::VisualMesh*       vm   = rr::make_visual("mammoth", mesh);

    std::vector<glm::vec3> colors1(mesh.num_faces());
    std::vector<glm::vec3> colors2(mesh.num_faces());
    std::vector<glm::vec3> colors3(mesh.num_faces());

    for (size_t i = 0; i < mesh.num_faces(); ++i) {
        colors1[i] = get_random_color();
        colors2[i] = get_random_color();
        colors3[i] = get_random_color();
    }

    vm->add_face_colors("colors1", colors1);
    vm->add_face_colors("colors2", colors2);
    vm->add_face_colors("colors3", colors3);

    // rr::set_user_callback(callback2);

    rr::show();

    return 0;
}
