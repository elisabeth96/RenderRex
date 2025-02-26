#pragma once

#include "glm/glm.hpp"
#include <memory>
#include <vector>
#include <webgpu/webgpu.h>

namespace rr {

class VisualMesh;
class InstancedMesh;
struct VisualMeshVertexAttributes;

class FaceVectorProperty {
public:
    explicit FaceVectorProperty(VisualMesh* vmesh, const std::vector<glm::vec3>& vs);

    void draw(WGPURenderPassEncoder);

    void on_camera_update();

    void set_color(const glm::vec3& color);

    void set_radius(float radius);

    void set_length(float length);

    void initialize_arrows(const std::vector<glm::vec3>& vectors);

    bool is_enabled() const {
        return m_is_enabled;
    }

    void set_enabled(bool enabled) {
        m_is_enabled = enabled;
    }

    glm::vec3 m_color = glm::vec3(0.882, 0.902, 0.376);
    // Note that radius and length are not absolute but with respect to m_scale.
    float m_scale  = 0.f;
    float m_radius = 1.f;
    float m_length = 1.f;

    bool m_instance_data_dirty = true;

    std::vector<glm::mat4> m_transforms;
    std::vector<glm::mat4> m_rigid;
    std::vector<glm::vec3> m_face_centers;
    std::vector<float>     m_vector_lengths;

    void update_instance_data();

    std::unique_ptr<InstancedMesh> m_arrows;

    VisualMesh* m_vmesh = nullptr;
    bool        m_is_enabled = false;
};

class FaceColorProperty {
public:
    explicit FaceColorProperty(VisualMesh* vmesh, const std::vector<glm::vec3>& colors);

    void set_colors(const std::vector<glm::vec3>& colors);

    bool is_enabled() const {
        return m_is_enabled;
    }

    void set_enabled(bool enabled) {
        m_is_enabled = enabled;
    }

    const std::vector<glm::vec3>& get_colors() const {
        return m_colors;
    }

private:
    VisualMesh* m_vmesh      = nullptr;
    bool        m_is_enabled = false;

    std::vector<glm::vec3> m_colors;
};

} // namespace rr