#include "Renderer.h"
#include "Drawable.h"
#include "VisualMesh.h"

#include "glfw3webgpu/glfw3webgpu.h"
#include <GLFW/glfw3.h>

#include "imguizmo/ImGuizmo.h"
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include <imguizmo/ImGuizmo.h>
#include <webgpu/webgpu.h>

#include <glm/gtc/type_ptr.hpp>

namespace rr {

WGPUStringView to_string_view(const char* str) {
    WGPUStringView view;
    view.data   = str;
    view.length = strlen(str);
    return view;
}

std::string to_string(const WGPUStringView& view) {
    return std::string(view.data, view.length);
}

WGPUAdapter request_adapter_sync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
    struct UserData {
        WGPUAdapter adapter       = nullptr;
        bool        request_ended = false;
    };
    UserData user_data;

    auto on_adapter_request_ended = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                                       void* p_user_data, void*) {
        auto& ud = *reinterpret_cast<UserData*>(p_user_data);
        if (status == WGPURequestAdapterStatus_Success) {
            ud.adapter = adapter;
        } else {
            std::string str(message.data, message.data + message.length);
            std::cerr << "Could not get WebGPU adapter: " << str << std::endl;
        }
        ud.request_ended = true;
    };

    // Important: Use AllowSpontaneous so Dawn can call back on its own thread.
    WGPURequestAdapterCallbackInfo callback_info = {/* userdataLabel  */ nullptr,
                                                    /* mode          */ WGPUCallbackMode_AllowSpontaneous,
                                                    /* callback      */ on_adapter_request_ended,
                                                    /* userdata      */ &user_data,
                                                    /* scope         */ nullptr};

    wgpuInstanceRequestAdapter(instance, options, callback_info);

    while (!user_data.request_ended) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }

    assert(user_data.request_ended);
    return user_data.adapter;
}

/**
 * Utility function to get a WebGPU texture view from a surface
 */
WGPUTextureView get_next_surface_texture_view(WGPUSurface surface) {
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(surface, &surface_texture);
    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return nullptr;
    }

    WGPUTextureViewDescriptor view_descriptor;
    view_descriptor.nextInChain     = nullptr;
    view_descriptor.label           = to_string_view("Surface texture view");
    view_descriptor.format          = wgpuTextureGetFormat(surface_texture.texture);
    view_descriptor.dimension       = WGPUTextureViewDimension_2D;
    view_descriptor.baseMipLevel    = 0;
    view_descriptor.mipLevelCount   = 1;
    view_descriptor.baseArrayLayer  = 0;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.aspect          = WGPUTextureAspect_All;

    WGPUTextureView target_view = wgpuTextureCreateView(surface_texture.texture, &view_descriptor);
    return target_view;
}

/**
 * Utility function to get a WebGPU device. It is very similar to requestAdapter
 */
WGPUDevice request_device_sync(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
    struct UserData {
        WGPUDevice device        = nullptr;
        bool       request_ended = false;
    };
    UserData user_data;

    WGPURequestDeviceCallback on_device_request_ended = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                                           WGPUStringView message, void* p_user_data, void*) {
        UserData& user_data = *reinterpret_cast<UserData*>(p_user_data);
        if (status == WGPURequestDeviceStatus_Success) {
            user_data.device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << to_string(message) << std::endl;
        }
        user_data.request_ended = true;
    };

    WGPURequestDeviceCallbackInfo callback_info = {nullptr, WGPUCallbackMode::WGPUCallbackMode_AllowSpontaneous,
                                                   on_device_request_ended, (void*)&user_data, nullptr};

    wgpuAdapterRequestDevice(adapter, descriptor, callback_info);

    while (!user_data.request_ended) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    }

    assert(user_data.request_ended);
    assert(user_data.device);

    return user_data.device;
}

void Renderer::terminate_gui() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}

Renderer::~Renderer() {
    // terminate GUI
    terminate_gui();

    // release resources
    wgpuTextureViewRelease(m_depth_texture_view);

    wgpuQueueRelease(m_queue);
    wgpuDeviceRelease(m_device);
    wgpuSurfaceUnconfigure(m_surface);
    wgpuSurfaceRelease(m_surface);
    wgpuInstanceRelease(m_instance);

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

WGPURenderPassEncoder Renderer::create_render_pass(WGPUTextureView next_texture, WGPUCommandEncoder encoder) {
    WGPURenderPassDescriptor render_pass_desc{};

    WGPURenderPassColorAttachment render_pass_color_attachment{};
    render_pass_color_attachment.view          = next_texture;
    render_pass_color_attachment.resolveTarget = nullptr;
    render_pass_color_attachment.loadOp        = WGPULoadOp_Clear;
    render_pass_color_attachment.storeOp       = WGPUStoreOp_Store;
    render_pass_color_attachment.clearValue    = WGPUColor{0.4, 0.4, 1, 1};
    render_pass_color_attachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;
    render_pass_color_attachment.nextInChain   = nullptr;
    render_pass_desc.colorAttachmentCount      = 1;
    render_pass_desc.colorAttachments          = &render_pass_color_attachment;
    render_pass_desc.nextInChain               = nullptr;

    WGPURenderPassDepthStencilAttachment depth_stencil_attachment = {};
    depth_stencil_attachment.view                                 = m_depth_texture_view;
    depth_stencil_attachment.depthClearValue                      = 1.0f;
    depth_stencil_attachment.depthLoadOp                          = WGPULoadOp_Clear;
    depth_stencil_attachment.depthStoreOp                         = WGPUStoreOp_Store;
    depth_stencil_attachment.depthReadOnly                        = false;
    depth_stencil_attachment.stencilClearValue                    = 0;
#ifdef WEBGPU_BACKEND_WGPU
    depth_stencil_attachment.stencilLoadOp  = LoadOp::Clear;
    depth_stencil_attachment.stencilStoreOp = StoreOp::Store;
#else
    depth_stencil_attachment.stencilLoadOp  = WGPULoadOp_Undefined;
    depth_stencil_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
#endif
    depth_stencil_attachment.stencilReadOnly = true;

    render_pass_desc.depthStencilAttachment = &depth_stencil_attachment;

    // render_pass_desc.timestampWriteCount = 0;
    render_pass_desc.timestampWrites = nullptr;

    return wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
}

void Renderer::update_gui(WGPURenderPassEncoder render_pass) {
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    ImGui::Begin("User Interface");

    int id = 0;
    if (ImGui::CollapsingHeader("Meshes")) {
        for (const auto& [name, mesh] : m_meshes) {

            ImGui::PushID(name.c_str());
            ImGui::Text(name.c_str());
            ImGui::SameLine();
            if(ImGui::Checkbox("Mesh", &mesh->m_visible_mesh)) {
                mesh->set_mesh_visible(mesh->m_visible_mesh);
            }
            ImGui::SameLine();
            if(ImGui::Checkbox("Wireframe", &mesh->m_show_wireframe)) {
                mesh->set_wireframe_visible(mesh->m_show_wireframe);
            }
            ImGui::SameLine();
            if (ImGui::Button("Options")) {
                mesh->m_show_options = !mesh->m_show_options;
            }

            mesh->update_ui(name, id);

            if (mesh->m_show_options && mesh->get_transform() != nullptr) {
                const char* transform_items[] = {"None", "Translate", "Rotate", "Scale"};
                int         current_item      = static_cast<int>(mesh->get_transform_status());
                std::string combo_label       = "Transform";
                if (ImGui::Combo(combo_label.c_str(), &current_item, transform_items, IM_ARRAYSIZE(transform_items))) {
                    mesh->set_transform_status(static_cast<TransformStatus>(current_item));
                }
            }

            ImGuizmo::PushID(id);
            handle_guizmo(mesh.get());
            ImGuizmo::PopID();

            ImGui::PopID();

            // Draw separator line between drawables
            if (id < m_meshes.size() - 1) {
                ImGui::Separator();
            }

            ++id;
        }
    }

    if (ImGui::CollapsingHeader("Point Clouds")) {
        for (const auto& [name, point_cloud] : m_point_clouds) {
            ImGui::PushID(name.c_str());
            point_cloud->update_ui(name, id);
            ImGui::PopID();
        }
    }

    if (ImGui::CollapsingHeader("Line Networks")) {
        for (const auto& [name, line_network] : m_line_networks) {
            ImGui::PushID(name.c_str());
            line_network->update_ui(name, id);
            ImGui::PopID();
        }
    }

    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);
}

void Renderer::update_frame() {
    glfwPollEvents();

    // get framebuffersize from glfw
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);

    if (m_user_callback) {
        m_user_callback();
    }
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(m_surface, &surface_texture);
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.nextInChain               = nullptr;
    view_desc.label                     = to_string_view("Surface texture view");
    view_desc.format                    = wgpuTextureGetFormat(surface_texture.texture);
    view_desc.dimension                 = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel              = 0;
    view_desc.mipLevelCount             = 1;
    view_desc.baseArrayLayer            = 0;
    view_desc.arrayLayerCount           = 1;
    view_desc.aspect                    = WGPUTextureAspect_All;

    WGPUTextureView next_texture = wgpuTextureCreateView(surface_texture.texture, &view_desc);
    if (!next_texture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        exit(1);
    }
    WGPUCommandEncoderDescriptor command_encoder_desc = {};
    command_encoder_desc.label                        = to_string_view("Command Encoder");
    WGPUCommandEncoder encoder                        = wgpuDeviceCreateCommandEncoder(m_device, &command_encoder_desc);

    WGPURenderPassEncoder render_pass = create_render_pass(next_texture, encoder);

    // Draw all drawables
    for (auto& mesh : m_meshes) {
        mesh.second->draw(render_pass);
    }
    for (auto& point_cloud : m_point_clouds) {
        point_cloud.second->draw(render_pass);
    }
    for (auto& line_network : m_line_networks) {
        line_network.second->draw(render_pass);
    }

    // Update GUI and Guizmo manipulators
    update_gui(render_pass);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    wgpuTextureViewRelease(next_texture);

    WGPUCommandBufferDescriptor cmd_buffer_descriptor{};
    cmd_buffer_descriptor.label = to_string_view("Command buffer");
    WGPUCommandBuffer command   = wgpuCommandEncoderFinish(encoder, &cmd_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuSurfacePresent(m_surface);
    wgpuTextureRelease(surface_texture.texture);

#ifdef WEBGPU_BACKEND_DAWN
    // Check for pending error callbacks
    wgpuDeviceTick(m_device);
#endif
}

bool Renderer::should_close() {
    return glfwWindowShouldClose(m_window);
}

Renderer& Renderer::get() {
    static Renderer instance;
    return instance;
}

VisualMesh* Renderer::register_mesh(std::string_view name, std::unique_ptr<VisualMesh> mesh) {
    auto& slot = m_meshes[std::string(name)];
    slot       = std::move(mesh);

    BoundingBox global_bb{};
    for (auto& p : m_meshes) {
        global_bb.expand_to_include(p.second->m_bbox);
    }

    glm::vec3 current_eye    = m_camera.eye();
    glm::vec3 current_center = m_camera.center();
    glm::vec3 center         = (global_bb.lower + global_bb.upper) * 0.5f;
    glm::vec3 offset         = current_eye - current_center;
    glm::vec3 new_eye        = center + offset;
    // m_camera                 = Camera(new_eye, center, m_camera.up());
    on_camera_update();

    return slot.get();
}

VisualPointCloud* Renderer::register_point_cloud(std::string_view name, std::unique_ptr<VisualPointCloud> point_cloud) {
    auto& slot = m_point_clouds[std::string(name)];
    slot       = std::move(point_cloud);

    BoundingBox global_bb{};
    for (auto& p : m_point_clouds) {
        global_bb.expand_to_include(p.second->m_bbox);
    }

    glm::vec3 current_eye    = m_camera.eye();
    glm::vec3 current_center = m_camera.center();
    glm::vec3 center         = (global_bb.lower + global_bb.upper) * 0.5f;
    glm::vec3 offset         = current_eye - current_center;
    glm::vec3 new_eye        = center + offset;
    // m_camera                 = Camera(new_eye, center, m_camera.up());
    on_camera_update();

    return slot.get();
}

VisualLineNetwork* Renderer::register_line_network(std::string_view                   name,
                                                   std::unique_ptr<VisualLineNetwork> line_network) {
    auto& slot = m_line_networks[std::string(name)];
    slot       = std::move(line_network);

    BoundingBox global_bb{};
    for (auto& p : m_line_networks) {
        global_bb.expand_to_include(p.second->m_bbox);
    }

    glm::vec3 current_eye    = m_camera.eye();
    glm::vec3 current_center = m_camera.center();
    glm::vec3 center         = (global_bb.lower + global_bb.upper) * 0.5f;
    glm::vec3 offset         = current_eye - current_center;
    glm::vec3 new_eye        = center + offset;
    // m_camera                 = Camera(new_eye, center, m_camera.up());
    on_camera_update();

    return slot.get();
}

void Renderer::set_user_callback(std::function<void()> callback) {
    m_user_callback = std::move(callback);
}

void Renderer::initialize_depth_texture() { // Create the depth texture
    m_depth_texture_format                   = WGPUTextureFormat_Depth24Plus;
    WGPUTextureDescriptor depth_texture_desc = {};
    depth_texture_desc.dimension             = WGPUTextureDimension_2D;
    depth_texture_desc.format                = m_depth_texture_format;
    depth_texture_desc.mipLevelCount         = 1;
    depth_texture_desc.sampleCount           = 1;
    depth_texture_desc.size                  = {m_width, m_height, 1};
    depth_texture_desc.usage                 = WGPUTextureUsage_RenderAttachment;
    depth_texture_desc.viewFormatCount       = 1;
    depth_texture_desc.viewFormats           = (WGPUTextureFormat*)&m_depth_texture_format;
    WGPUTexture depth_texture                = wgpuDeviceCreateTexture(m_device, &depth_texture_desc);

    // Create the view of the depth texture manipulated by the rasterizer
    WGPUTextureViewDescriptor depth_texture_view_desc = {};
    depth_texture_view_desc.aspect                    = WGPUTextureAspect_DepthOnly;
    depth_texture_view_desc.baseArrayLayer            = 0;
    depth_texture_view_desc.arrayLayerCount           = 1;
    depth_texture_view_desc.baseMipLevel              = 0;
    depth_texture_view_desc.mipLevelCount             = 1;
    depth_texture_view_desc.dimension                 = WGPUTextureViewDimension_2D;
    depth_texture_view_desc.format                    = m_depth_texture_format;
    m_depth_texture_view                              = wgpuTextureCreateView(depth_texture, &depth_texture_view_desc);
}

void Renderer::initialize_window() {
    // initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }

    // create a window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(m_width, m_height, "RenderRex", NULL, NULL);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Add window callbacks
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        renderer->resize(width, height);
    });

    glfwSetWindowUserPointer(m_window, this);
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr)
            that->on_mouse_move(xpos, ypos);
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr)
            that->on_mouse_button(button, action, mods);
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr)
            that->on_scroll(xoffset, yoffset);
    });
}

void Renderer::initialize_device() {
    // We create a WGPU descriptor
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain            = nullptr;

    // We create the instance using this descriptor
    m_instance = wgpuCreateInstance(&desc);

    // We can check whether there is actually an instance created
    if (!m_instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        exit(EXIT_FAILURE);
    }

    m_surface = glfwGetWGPUSurface(m_instance, m_window);

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain               = nullptr;
    adapterOpts.compatibleSurface         = m_surface;
    WGPUAdapter adapter                   = request_adapter_sync(m_instance, &adapterOpts);

    WGPUDeviceDescriptor deviceDesc     = {};
    deviceDesc.nextInChain              = nullptr;
    deviceDesc.label                    = to_string_view("My Device"); // anything works here, that's your call
    deviceDesc.requiredFeatureCount     = 0;                           // we do not require any specific feature
    deviceDesc.requiredLimits           = nullptr;                     // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label       = to_string_view("The default queue");
    // deviceDesc.deviceLostCallback       = nullptr;

    m_device = request_device_sync(adapter, &deviceDesc);

    // auto on_device_error = [](WGPUErrorType type, WGPUStringView message, void* /* pUserData */, void*) {
    //     std::cout << "Uncaptured device error: type " << type;
    //     if (message.length > 0)
    //         std::cout << " (" << to_string(message) << ")";
    //     std::cout << std::endl;
    // };

    // wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
    wgpuAdapterRelease(adapter);
}

void Renderer::configure_surface() {
    m_swap_chain_format             = WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration config = {};
    config.device                   = m_device;
    config.format                   = m_swap_chain_format;
    config.usage                    = WGPUTextureUsage_RenderAttachment;
    config.width                    = m_width;
    config.height                   = m_height;
    config.presentMode              = WGPUPresentMode_Fifo;
    config.nextInChain              = nullptr;
    wgpuSurfaceConfigure(m_surface, &config);
}

void Renderer::initialize_queue() {
    m_queue = wgpuDeviceGetQueue(m_device);
    if (!m_queue) {
        std::cerr << "Failed to get device queue" << std::endl;
        exit(2);
    }
}

void Renderer::initialize_gui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(m_window, true);
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device                  = m_device;
    init_info.RenderTargetFormat      = m_swap_chain_format;
    init_info.DepthStencilFormat      = m_depth_texture_format;
    ImGui_ImplWGPU_Init(&init_info);

    // Initialize ImGuizmo
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
}

Renderer::Renderer() : m_camera({0, 0, 5}, {0, 0, 0}, {0, 1, 0}) {
    initialize_window();
    initialize_device();
    initialize_queue();

    // m_width and m_height are used to create the window, but that is not necessarily the same as the framebuffer size.
    // so we update them before configuring the surface in case they are not the same.
    int fb_width, fb_height;
    glfwGetFramebufferSize(m_window, &fb_width, &fb_height);
    m_width  = fb_width;
    m_height = fb_height;
    update_projection();

    configure_surface();
    initialize_depth_texture();
    initialize_gui();
}

void Renderer::update_projection() {
    float aspect_ratio = static_cast<float>(m_width) / static_cast<float>(m_height);
    float far_plane    = 100.0f;
    float near_plane   = 0.01f;
    float fov          = glm::radians(45.0f);
    m_projection       = glm::perspective(fov, aspect_ratio, near_plane, far_plane);
}

void Renderer::on_camera_update() {
    update_projection();

    for (auto& mesh : m_meshes) {
        mesh.second->on_camera_update();
    }
    for (auto& point_cloud : m_point_clouds) {
        point_cloud.second->on_camera_update();
    }
    for (auto& line_network : m_line_networks) {
        line_network.second->on_camera_update();
    }
}

glm::vec2 transform_mouse(glm::vec2 in, uint32_t width, uint32_t height) {
    return {in.x * 2.f / float(width) - 1.f, 1.f - 2.f * in.y / float(height)};
}

void Renderer::on_mouse_move(double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    if (m_drag.active) {
        glm::vec2 current_pos = transform_mouse({xpos, ypos}, m_width, m_height);
        glm::vec2 last_pos    = m_drag.last_pos;

        if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            m_camera.rotate(last_pos, current_pos);
        } else if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            glm::vec2 delta = (current_pos - last_pos) * m_drag.pan_speed;
            m_camera.pan(delta);
        }

        m_drag.last_pos = current_pos;
        on_camera_update();
    }
}

void Renderer::on_mouse_button(int button, int action, int /* modifiers */) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
        switch (action) {
        case GLFW_PRESS:
            m_drag.active = true;
            double x, y;
            glfwGetCursorPos(m_window, &x, &y);
            m_drag.last_pos = transform_mouse({x, y}, m_width, m_height);
            break;
        case GLFW_RELEASE:
            m_drag.active = false;
            break;
        }
    }
}

void Renderer::on_scroll(double /* xoffset */, double yoffset) {
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    m_camera.zoom(static_cast<float>(yoffset) * m_drag.scroll_sensitivity);
    on_camera_update();
}

void Renderer::resize(int width, int height) {
    m_width  = width;
    m_height = height;

    configure_surface();

    if (m_depth_texture_view) {
        wgpuTextureViewRelease(m_depth_texture_view);
    }

    initialize_depth_texture();

    on_camera_update();
}

void Renderer::handle_guizmo(Drawable* drawable) {
    // Get view and projection matrices
    glm::mat4 view = m_camera.transform();
    glm::mat4 proj = m_projection;

    TransformStatus status = drawable->get_transform_status();
    if (status == TransformStatus::None)
        return;

    glm::mat4 transform = *drawable->get_transform();

    ImGuizmo::OPERATION operation;
    switch (status) {
    case TransformStatus::Translation:
        operation = ImGuizmo::TRANSLATE;
        break;
    case TransformStatus::Rotation:
        operation = ImGuizmo::ROTATE;
        break;
    case TransformStatus::Scale:
        operation = ImGuizmo::SCALE;
        break;
    default:
        return;
    }

    // Manipulate
    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), operation, ImGuizmo::WORLD,
                             glm::value_ptr(transform))) {

        drawable->set_transform(transform);
    }
}

} // namespace rr
