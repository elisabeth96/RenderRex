// contains all the unser interface funtions for the renderrex library
#pragma once

#include "Camera.h"
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rr {

class Drawable;
class VisualMesh;
struct Mesh;

class Renderer {
public:
    uint32_t     m_width  = 1000;
    uint32_t     m_height = 1000;
    WGPUDevice   m_device;
    WGPUQueue    m_queue;
    WGPUInstance m_instance;
    // WGPUSwapChain     m_swapChain;
    WGPUTextureView   m_depthTextureView;
    WGPUTextureFormat m_swapChainFormat;
    WGPUTextureFormat m_depthTextureFormat;

    Camera m_camera;

    // Mouse drag state
    struct {
        bool      active = false;
        glm::vec2 last_pos;
        float     rotationSpeed     = 0.02f;
        float     panSpeed          = 1.f;
        float     scrollSensitivity = 0.2f;
    } m_drag;

    ~Renderer();

    // delete copy constructor and assignment operator
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    WGPURenderPassEncoder create_render_pass(WGPUTextureView nextTexture, WGPUCommandEncoder encoder);

    void update_frame();

    bool should_close();

    static Renderer& get();

    Drawable* register_drawable(std::string_view name, std::unique_ptr<Drawable> drawable);

    void set_user_callback(std::function<void()> callback);

    // Mouse events
    void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

private:
    Renderer();

    void initialize_window();
    void initialize_device();
    void configure_surface();
    void initialize_queue();
    void initialize_depth_texture();
    void initialize_gui();

    void on_camera_update();
    void resize(int width, int height);

    GLFWwindow* m_window;
    WGPUSurface m_surface;

    std::unordered_map<std::string, std::unique_ptr<Drawable>> m_drawables;

    std::function<void()> m_user_callback;

    void terminate_gui();                              // called in onFinish
    void update_gui(WGPURenderPassEncoder renderPass); // called in onFrame
};

WGPUStringView to_string_view(const char* str);
std::string    to_string(const WGPUStringView& view);

} // namespace rr
