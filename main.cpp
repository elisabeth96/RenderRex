#include "RenderRex.h"

#include <fstream>
#include <sstream>
#include <string>

//extern const char spot_data[];

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

rr::FaceVectorAttribute* fa = nullptr;

void callback() {
    static bool is_red = true;
    if (is_red) {
        fa->set_color({0, 1, 0});
    } else {
        fa->set_color({1, 0, 0});
    }
    is_red = !is_red;
}

int main() {
    // std::vector<glm::vec3>          positions;
    // std::vector<std::array<int, 3>> triangles;

    // TODO: with the current system this generates too much geometry
    // std::string path  = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    // rr::Mesh mesh = rr::load_mesh(path);
    // std::stringstream mesh_data(spot_data);
    // rr::Mesh          mesh = rr::load_mesh(mesh_data);
    // rr::VisualMesh*   rm   = rr::make_visual("mesh", mesh);
    // fa                     = rm->add_face_attribute("face normals", compute_face_normals(mesh));
    // rr::set_user_callback(callback);

    float                  offset = 2.f;
    std::vector<glm::vec3> positions(9);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            positions[i * 3 + j] = {offset * float(i), offset * float(j), 0};
        }
    }

    // 9 random colors
    std::vector<glm::vec4> colors = {
        glm::vec4(0.93f, 0.26f, 0.36f, 1.0f), // Soft red
        glm::vec4(0.36f, 0.65f, 0.93f, 1.0f), // Sky blue
        glm::vec4(0.45f, 0.76f, 0.29f, 1.0f), // Fresh green
        glm::vec4(0.98f, 0.81f, 0.27f, 1.0f), // Warm yellow
        glm::vec4(0.56f, 0.33f, 0.78f, 1.0f), // Deep purple
        glm::vec4(0.98f, 0.50f, 0.45f, 1.0f), // Coral
        glm::vec4(0.26f, 0.56f, 0.89f, 1.0f), // Rich blue
        glm::vec4(0.13f, 0.59f, 0.53f, 1.0f), // Teal
        glm::vec4(0.87f, 0.87f, 0.87f, 1.0f)  // Light gray
    };

    rr::Mesh m = rr::create_sphere();
    rr::set_flat_normals(m);
    rr::InstancedMesh* im = rr::make_instanced("im", m, 9);
    im->set_translations(positions);
    im->set_colors(colors);

    //rr::make_visual("mesh", box);
    //save_obj("/Users/jmeny/box.obj", box);

    rr::show();

    // rr::Mesh mesh = rr::create_sphere();
    // rr::set_flat_normals(mesh);
    // rr::RenderMesh* rm = rr::register_mesh("mesh", mesh);
    // rm->add_face_attribute("face normals", mesh.normals);

    // rr::register_mesh("sphere", rr::create_sphere());
    // rr::register_mesh("arrow", create_arrow());
    // rr::register_mesh("box", rr::create_box().translate({-1.5f, 0.f, -0.5f}));
    // rr::register_mesh("cylinder", rr::create_cylinder().translate({1.5f, 0.f, -0.5f}));
    // rr::register_mesh("cone", rr::create_cone().translate({0.f, 0.f, 1.5f,}));

    rr::show();

    return 0;
}
