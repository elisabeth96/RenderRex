// contains all the unser interface funtions for the renderrex library
#pragma once

#include "BoundingBox.h"
#include <string>
#include <webgpu/webgpu.h>
#include <glm/glm.hpp>

namespace rr {

class Renderer;

enum class TransformStatus {
    None, 
    Translation,
    Rotation,
    Scale
};

class Drawable {
public:
    Drawable(const Renderer* r, BoundingBox bb) : m_renderer(r), m_bbox(bb) {}

    virtual ~Drawable() = default;

    virtual void draw(WGPURenderPassEncoder renderPass) = 0;

    virtual void on_camera_update() = 0;

    virtual void update_ui(std::string name, int index) = 0;

    virtual void set_transform(const glm::mat4&) {}

    virtual const glm::mat4* get_transform() const { return nullptr; }
    
    TransformStatus get_transform_status() const { return m_transform_status; }

    void set_transform_status(TransformStatus status) { m_transform_status = status; }

    const Renderer* m_renderer = nullptr;
    BoundingBox     m_bbox;

    bool m_visible = true;

    TransformStatus m_transform_status{TransformStatus::None};
};

} // namespace rr
