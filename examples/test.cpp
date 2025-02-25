#include "RenderRex.h"
#include "Utils.h"
#include <random> // Add include for random number generation

int main() {
    rr::Mesh spot = rr::load_mesh(std::string(RESOURCE_DIR) + "/spot.obj");
    rr::VisualPointCloud* vpc = rr::make_visual("spot_points", spot.positions);

// Define the 8 corners of a cube centered at the origin, with edge length 1.0f.
    std::vector<glm::vec3> positions = {
        glm::vec3(-0.5f, -0.5f, -0.5f), // 0
        glm::vec3( 0.5f, -0.5f, -0.5f), // 1
        glm::vec3( 0.5f,  0.5f, -0.5f), // 2
        glm::vec3(-0.5f,  0.5f, -0.5f), // 3
        glm::vec3(-0.5f, -0.5f,  0.5f), // 4
        glm::vec3( 0.5f, -0.5f,  0.5f), // 5
        glm::vec3( 0.5f,  0.5f,  0.5f), // 6
        glm::vec3(-0.5f,  0.5f,  0.5f)  // 7
    };

    // Define the 12 edges of the cube as pairs of vertex indices.
    std::vector<std::pair<int, int>> lines = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom face
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Top face
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Vertical edges
    };

    // Create and show the VisualLineNetwork of the cube.
    rr::VisualLineNetwork* cube = rr::make_visual("CubeNetwork", positions, lines);

    std::string path = std::string(RESOURCE_DIR) + "/mammoth_simple.obj";
    rr::Mesh mesh = rr::load_mesh(path);

    auto sphere = rr::create_sphere(10, 10).scale(0.1f);

    rr::VisualMesh* vm = rr::make_visual("mammoth", mesh);
    
    rr::VisualMesh* vs = rr::make_visual("sphere", sphere);

    // make vector property (face normals)
    std::vector<glm::vec3> face_normals;
    for(auto& face : mesh.position_faces) {
        auto normal = glm::normalize(glm::cross(mesh.positions[face[1]] - mesh.positions[face[0]], mesh.positions[face[2]] - mesh.positions[face[0]]));
        face_normals.push_back(normal);
    }
    auto face_normals_prop = vm->add_face_vectors("face_normals", face_normals);

    // Create random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.1f, 10.0f);
    
    // make another vector property (normals but with random length)
    std::vector<glm::vec3> face_normals_random_length;
    for(size_t i = 0; i < mesh.position_faces.size(); ++i) {
        face_normals_random_length.push_back(face_normals[i] * dist(gen));
    }
    auto face_normals_random_length_prop = vm->add_face_vectors("face_normals_random_length", face_normals_random_length);

    // make face color property with random colors
    std::vector<glm::vec3> face_colors;
    for(size_t i = 0; i < mesh.position_faces.size(); ++i) {
        face_colors.push_back(rr::get_random_color());
    }
    auto face_colors_prop = vm->add_face_colors("face_colors", face_colors);
    // make another face color property with random colors
    std::vector<glm::vec3> face_colors2;
    for(size_t i = 0; i < mesh.position_faces.size(); ++i) {
        face_colors2.push_back(rr::get_random_color());
    }
    auto face_colors_prop2 = vm->add_face_colors("face_colors2", face_colors2);
    
    
    rr::show();

    return 0;
}
