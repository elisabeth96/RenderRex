// contains all the unser interface funtions for the renderrex library
#pragma once
#include "glm/glm.hpp"
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rr {

class Drawable;

class Renderer {
public:
    const uint32_t    m_width  = 2000;
    const uint32_t    m_height = 2000;
    WGPUDevice        m_device;
    WGPUQueue         m_queue;
    WGPUSwapChain     m_swapChain;
    WGPUTextureView   m_depthTextureView;
    WGPUTextureFormat m_swapChainFormat;
    WGPUTextureFormat m_depthTextureFormat;

    ~Renderer();

    // delete copy constructor and assignment operator
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    void update_frame();

    bool should_close();

    static Renderer& get_renderer();

    void register_mesh(std::string name, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles);

private:
    Renderer();

    void configure_depth_texture();

    GLFWwindow* m_window;
    WGPUSurface m_surface;

    std::unordered_map<std::string, std::unique_ptr<Drawable>> m_drawables;
};

} // namespace rr
