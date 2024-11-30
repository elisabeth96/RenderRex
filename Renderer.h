// contains all the unser interface funtions for the renderrex library
#pragma once

#include "Camera.h"
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
    const uint32_t    m_width  = 1000;
    const uint32_t    m_height = 1000;
    WGPUDevice        m_device;
    WGPUQueue         m_queue;
    WGPUSwapChain     m_swapChain;
    WGPUTextureView   m_depthTextureView;
    WGPUTextureFormat m_swapChainFormat;
    WGPUTextureFormat m_depthTextureFormat;

    Camera m_camera;

    // Mouse drag state
    struct {
        bool active = false;
        glm::vec2 last_pos;
        float rotationSpeed = 0.01f;
        float panSpeed = 0.01f;
        float scrollSensitivity = 0.1f;
    } m_drag;

    ~Renderer();

    // delete copy constructor and assignment operator
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    WGPURenderPassEncoder create_render_pass(WGPUTextureView nextTexture, WGPUCommandEncoder encoder);

    void update_frame();

    bool should_close();

    static Renderer& get();

    void register_mesh(std::string name, std::vector<glm::vec3>& positions, std::vector<std::array<int, 3>>& triangles);

    // Mouse events
    void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

private:
    Renderer();

    void initialize_window();
    void initialize_device();
    void initialize_swap_chain();
    void initialize_queue();
    void initialize_depth_texture();

    void on_camera_update();

    GLFWwindow* m_window;
    WGPUSurface m_surface;

    std::unordered_map<std::string, std::unique_ptr<Drawable>> m_drawables;
};

} // namespace rr
