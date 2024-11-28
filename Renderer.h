// contains all the unser interface funtions for the renderrex library
#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
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

struct CameraState {
    // angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
    // angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
    glm::vec2 angles = {0.8f, 0.5f};
    // zoom is the position of the camera along its local forward axis, affected by the scroll wheel
    float zoom = -1.2f;
};

struct DragState {
    // Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
    bool active = false;
    // The position of the mouse at the beginning of the drag action
    glm::vec2 startMouse;
    // The camera state at the beginning of the drag action
    CameraState startCameraState;

    // Constant settings
    float sensitivity       = 0.01f;
    float scrollSensitivity = 0.1f;

    // Inertia
    glm::vec2 velocity = {0.0, 0.0};
    glm::vec2 previousDelta;
    float     intertia = 0.9f;
};

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

    void updateViewMatrix();

    void updateDragInertia();

    GLFWwindow* m_window;
    WGPUSurface m_surface;
    CameraState m_cameraState;
    DragState   m_drag;

    std::unordered_map<std::string, std::unique_ptr<Drawable>> m_drawables;
};

} // namespace rr
