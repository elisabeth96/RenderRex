#include "Property.h"

#include "InstancedMesh.h"
#include "Primitives.h"
#include "VisualMesh.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"

namespace rr {

FaceVectorProperty::FaceVectorProperty(VisualMesh* vmesh, const std::vector<glm::vec3>& vectors) : Property(vmesh) {

    const Mesh&            mesh = m_vmesh->m_mesh;
    std::vector<glm::vec3> face_centers(mesh.num_faces());

    double average_edge_length = 0.0;
    size_t total_edges         = 0;

    // Calculate face centers and average edge length
    for (size_t i = 0; i < mesh.num_faces(); ++i) {
        const auto& f = mesh.position_faces[i];

        // Calculate face center
        glm::vec3 center(0.0f);
        for (size_t j = 0; j < f.size(); ++j) {
            center += mesh.positions[f[j]];
        }
        center /= float(f.size());
        face_centers[i] = center;

        // Calculate length of each edge in the face
        for (size_t j = 0; j < f.size(); ++j) {
            const glm::vec3& p0          = mesh.positions[f[j]];
            const glm::vec3& p1          = mesh.positions[f[(j + 1) % f.size()]];
            float            edge_length = glm::length(p1 - p0);
            total_edges++;
            double delta = edge_length - average_edge_length;
            average_edge_length += delta / total_edges;
        }
    }

    m_scale = float(average_edge_length);
    initialize_arrows(vectors, face_centers);
}

void FaceVectorProperty::initialize_arrows(const std::vector<glm::vec3>& vectors,
                                           const std::vector<glm::vec3>& face_centers) {
    // Create base meshes for arrow
    Mesh cylinder = create_cylinder().triangulate();
    Mesh cone     = create_cone().triangulate();

    // Combine cylinder and cone into single arrow mesh
    Mesh arrow_mesh;

    // Scale factors for cylinder and cone
    float cylinder_radius = 0.05f;
    float cylinder_length = 0.7f;
    float cone_radius     = 0.15f;
    float cone_length     = 0.3f;

    // Transform cylinder
    glm::mat4 cylinder_scale = glm::scale(glm::vec3(cylinder_radius, cylinder_length, cylinder_radius));
    glm::vec3 cylinder_offset(0.0f, cylinder_length * 0.5f, 0.0f);
    glm::mat4 cylinder_transform = glm::translate(cylinder_offset) * cylinder_scale;

    // Transform cone
    glm::mat4 cone_scale = glm::scale(glm::vec3(cone_radius, cone_length, cone_radius));
    glm::vec3 cone_offset(0.0f, cylinder_length + cone_length * 0.5f, 0.0f);
    glm::mat4 cone_transform = glm::translate(cone_offset) * cone_scale;

    // Add transformed cylinder vertices
    for (const auto& pos : cylinder.positions) {
        arrow_mesh.positions.push_back(glm::vec3(cylinder_transform * glm::vec4(pos, 1.0f)));
    }
    for (const auto& normal : cylinder.normals) {
        arrow_mesh.normals.push_back(normal);
    }
    for (const auto& face : cylinder.position_faces) {
        arrow_mesh.position_faces.push_back(face);
    }
    for (const auto& face : cylinder.normal_faces) {
        arrow_mesh.normal_faces.push_back(face);
    }

    // Add transformed cone vertices
    size_t base_vertex_count = arrow_mesh.positions.size();
    size_t base_normal_count = arrow_mesh.normals.size();
    for (const auto& pos : cone.positions) {
        arrow_mesh.positions.push_back(glm::vec3(cone_transform * glm::vec4(pos, 1.0f)));
    }
    for (const auto& normal : cone.normals) {
        arrow_mesh.normals.push_back(normal);
    }
    for (const auto& face : cone.position_faces) {
        Mesh::Face new_face;
        for (size_t idx : face) {
            new_face.push_back(idx + base_vertex_count);
        }
        arrow_mesh.position_faces.push_back(new_face);
    }
    for (const auto& face : cone.normal_faces) {
        Mesh::Face new_face;
        for (size_t idx : face) {
            new_face.push_back(idx + base_normal_count);
        }
        arrow_mesh.normal_faces.push_back(new_face);
    }

    // Create instanced mesh
    m_arrows = std::make_unique<InstancedMesh>(arrow_mesh, vectors.size(), *m_vmesh->m_renderer);
    m_arrows->set_color(glm::vec4(m_color, 1.0f));

    // Cache transform components
    size_t n = vectors.size();
    m_rotations.resize(n);
    m_translations = face_centers; // Already computed face centers
    m_vector_lengths.resize(n);
    m_transforms.resize(n);

    for (size_t i = 0; i < n; ++i) {
        glm::vec3 v            = vectors[i];
        m_vector_lengths[i]    = glm::length(v);
        glm::vec3 v_normalized = m_vector_lengths[i] > 0 ? v / m_vector_lengths[i] : glm::vec3(0, 1, 0);

        // Calculate rotation from y-axis to vector direction
        glm::vec3 y(0, 1, 0);
        glm::vec3 axis        = glm::cross(y, v_normalized);
        float     axis_length = glm::length(axis);
        float     angle       = 0.0f;

        if (axis_length < 1e-6f) {
            axis  = glm::vec3(1, 0, 0);
            angle = (v_normalized.y < 0) ? glm::pi<float>() : 0.0f;
        } else {
            axis  = axis / axis_length;
            angle = acos(glm::dot(y, v_normalized));
        }

        m_rotations[i] = glm::rotate(angle, axis);
    }

    // Initialize transforms
    update_transforms();
}

void FaceVectorProperty::update_transforms() {
    for (size_t i = 0; i < m_vector_lengths.size(); ++i) {
        float length_scale = m_vector_lengths[i] * m_scale * m_length;
        float radius_scale = m_radius * m_scale;

        glm::mat4 scale_matrix = glm::scale(glm::vec3(radius_scale, length_scale, radius_scale));
        m_transforms[i]        = glm::translate(m_translations[i]) * m_rotations[i] * scale_matrix;
    }

    m_arrows->set_transforms(m_transforms);
}

FaceVectorProperty::~FaceVectorProperty() = default;

void FaceVectorProperty::draw(WGPURenderPassEncoder pass) {
    m_arrows->draw(pass);
}

void FaceVectorProperty::on_camera_update() {
    m_arrows->on_camera_update();
}

void FaceVectorProperty::set_color(const glm::vec3& color) {
    m_color = color;
    m_arrows->set_color(glm::vec4(m_color, 1.0f));
}

void FaceVectorProperty::set_radius(float radius) {
    m_radius = radius;
    update_transforms();
}

void FaceVectorProperty::set_length(float length) {
    m_length = length;
    update_transforms();
}

} // namespace rr