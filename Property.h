#pragma once

#include "glm/glm.hpp"
#include <memory>
#include <vector>
#include <webgpu/webgpu.h>

namespace rr {

class VisualMesh;
class InstancedMesh;

class Property {
public:
    explicit Property(VisualMesh* vmesh) : m_vmesh(vmesh) {}

    virtual ~Property() = default;
    // virtual void generate_attributes(std::vector<VisualMeshVertexAttributes>&) = 0;
    virtual void draw(WGPURenderPassEncoder) = 0;

    virtual void on_camera_update() {}

    VisualMesh* m_vmesh = nullptr;
};

class FaceVectorProperty : public Property {
public:
    explicit FaceVectorProperty(VisualMesh* vmesh, const std::vector<glm::vec3>& vs);
    ~FaceVectorProperty() override;

    void draw(WGPURenderPassEncoder) override;

    void on_camera_update() override;

    void set_color(const glm::vec3& color);

    void set_radius(float radius);

    void set_length(float length);

    void initialize_arrows(const std::vector<glm::vec3>& vectors, const std::vector<glm::vec3>& face_centers);

    glm::vec3 m_color = glm::vec3(0.882, 0.902, 0.376);
    // Note that radius and length are not absolute but with respect to m_scale.
    float m_scale  = 0.f;
    float m_radius = 1.f;
    float m_length = 1.f;

    bool m_instance_data_dirty = true;

    std::vector<glm::mat4> m_transforms;
    std::vector<glm::mat4> m_rigid;
    std::vector<float>     m_vector_lengths;

    void update_instance_data();

    std::unique_ptr<InstancedMesh> m_arrows;
};

} // namespace rr