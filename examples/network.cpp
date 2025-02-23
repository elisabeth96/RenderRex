#include "RenderRex.h"

int main() {
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
    rr::show();

    return 0;
}
