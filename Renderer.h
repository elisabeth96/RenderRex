// contains all the unser interface funtions for the renderrex library
#pragma once

#include "Camera.h"
#include "BoundingBox.h"
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>


#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct ImGuiIO;

namespace rr {

class Drawable;
class VisualMesh;
struct Mesh;

class Renderer {
public:
    // framebuffer size, not that this is not necessarily the same as the window size
    uint32_t     m_width  = 1000;
    uint32_t     m_height = 1000;

    WGPUDevice   m_device;
    WGPUQueue    m_queue;
    WGPUInstance m_instance;
    // WGPUSwapChain     m_swapChain;
    WGPUTextureView   m_depth_texture_view;
    WGPUTextureFormat m_swap_chain_format;
    WGPUTextureFormat m_depth_texture_format;

    Camera m_camera;
    glm::mat4 m_projection;

    // Mouse drag state
    struct {
        bool      active = false;
        glm::vec2 last_pos;
        float     rotation_speed     = 0.02f;
        float     pan_speed          = 1.f;
        float     scroll_sensitivity = 0.2f;
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
    void on_mouse_move(double xpos, double ypos);
    void on_mouse_button(int button, int action, int mods);
    void on_scroll(double xoffset, double yoffset);

public:
    Renderer();

    void initialize_window();
    void initialize_device();
    void configure_surface();
    void initialize_queue();
    void initialize_depth_texture();
    void initialize_gui();
    void initialize_guizmo();

    void update_projection();
    void on_camera_update();
    void resize(int width, int height);

    GLFWwindow* m_window;
    WGPUSurface m_surface;

    std::unordered_map<std::string, std::unique_ptr<Drawable>> m_drawables;

    std::function<void()> m_user_callback;

    void terminate_gui();                              // called in onFinish
    void update_gui(WGPURenderPassEncoder renderPass); // called in onFrame
    void handle_guizmo(Drawable* drawable);
};

WGPUStringView to_string_view(const char* str);
std::string    to_string(const WGPUStringView& view);

} // namespace rr
