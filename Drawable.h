// contains all the unser interface funtions for the renderrex library
#pragma once

#include "BoundingBox.h"
#include <string>
#include <webgpu/webgpu.h>

namespace rr {

class Renderer;

class Drawable {
public:
    Drawable(const Renderer* r, BoundingBox bb) : m_renderer(r), m_bbox(bb) {}

    virtual ~Drawable() = default;

    virtual void draw(WGPURenderPassEncoder renderPass) = 0;

    virtual void on_camera_update() = 0;

    virtual void update_ui(std::string name, int index) = 0;

    const Renderer* m_renderer = nullptr;
    BoundingBox     m_bbox;

    bool m_visable = true;
};

} // namespace rr
