// contains all the unser interface funtions for the renderrex library

#include "Renderer.h"
#include "Drawable.h"
#include "glfw3webgpu/glfw3webgpu.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>

#include <cassert>
#include <chrono>
#include <thread>

#include <iostream>
#include <webgpu/webgpu.h>

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
        WGPUAdapter adapter = nullptr;
        bool request_ended = false;
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
    WGPURequestAdapterCallbackInfo callback_info = {
        /* userdataLabel  */ nullptr,
        /* mode          */ WGPUCallbackMode_AllowSpontaneous,
        /* callback      */ on_adapter_request_ended,
        /* userdata      */ &user_data,
        /* scope         */ nullptr
    };

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
        WGPUDevice device = nullptr;
        bool request_ended = false;
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
    render_pass_desc.colorAttachmentCount     = 1;
    render_pass_desc.colorAttachments         = &render_pass_color_attachment;
    render_pass_desc.nextInChain              = nullptr;

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

    // [...] Build our UI
    // static float radius = 1.0f;

    // static std::vector<bool> show_point_cloud(m_drawables.size(), true);
    //  static bool   show_another_window = false;
    // static ImVec4 color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ImGui::Begin("User Interface"); // Create a window called "Hello, world!" and append into it.

    ImGui::Text("Select your drawables"); // Display some text (you can use a format strings too)
                                          // Use an index that resets every frame.
    /* int index = 0;
    // For each drawable in the map, create a checkbox.
    for (const auto& pair : m_drawables) {
        // You might use the key name in the label.
        std::string label = pair.first + "##" + std::to_string(index);
        // Copy the value into a temporary bool.
        bool visible = show_point_cloud[index];

        // Use the temporary variable for the checkbox.
        if (ImGui::Checkbox(label.c_str(), &visible)) {
            // If the checkbox value changed, update the vector.
            show_point_cloud[index] = visible;
        }
        ++index;
    }*/
    int index = 0;
    for (const auto& pair : m_drawables) {
        pair.second->update_ui(pair.first, index);
        ++index;
    }

    // ImGui::SliderFloat("scale radius", &radius, 0.5f, 1.5f); // Edit 1 float using a slider from 0.0f to 1.0f
    // ImGui::ColorEdit3("change color", (float*)&color);       // Edit 3 floats representing a color

    // if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
    // counter++;
    // ImGui::SameLine();
    // ImGui::Text("counter = %d", counter);

    // ImGuiIO& io = ImGui::GetIO();
    // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::End();
    // go through drawables and change their radius and color
    /*glm::vec4 color_vec = {color.x, color.y, color.z, color.w};
    int       i         = 0;
    for (auto& drawable : m_drawables) {
        if (show_point_cloud[i]) {
            drawable.second->updateFromUI(color_vec, radius);
        } else {
            drawable.second->updateFromUI(glm::vec4(0.0f), 0.0f);
        }
        ++i;
    }*/

    // Draw the UI
    ImGui::EndFrame();
    // Convert the UI defined above into low-level drawing commands
    ImGui::Render();
    // Execute the low-level drawing commands on the WebGPU backend
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), render_pass);
}

void Renderer::update_frame() {
    glfwPollEvents();

    // get framebuffersize from glfw
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    //printf("Framebuffer size: %d %d, width: %d, height: %d\n", width, height, m_width, m_height);

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
    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.label                        = to_string_view("Command Encoder");
    WGPUCommandEncoder encoder                      = wgpuDeviceCreateCommandEncoder(m_device, &commandEncoderDesc);

    WGPURenderPassEncoder render_pass = create_render_pass(next_texture, encoder);

    for (auto& drawable : m_drawables) {
        drawable.second->draw(render_pass);
    }

    // We add the GUI drawing commands to the render pass
    update_gui(render_pass);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    wgpuTextureViewRelease(next_texture);

    WGPUCommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = to_string_view("Command buffer");
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
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

Drawable* Renderer::register_drawable(std::string_view name, std::unique_ptr<Drawable> drawable) {
    auto& slot = m_drawables[std::string(name)];
    slot       = std::move(drawable);

    BoundingBox global_bb{};
    for (auto& p : m_drawables) {
        global_bb.expand_to_include(p.second->m_bbox);
    }

    glm::vec3 current_eye    = m_camera.eye();
    glm::vec3 current_center = m_camera.center();
    glm::vec3 center         = (global_bb.lower + global_bb.upper) * 0.5f;
    glm::vec3 offset         = current_eye - current_center;
    glm::vec3 new_eye        = center + offset;
    m_camera                 = Camera(new_eye, center, m_camera.up());
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
    WGPUTexture depth_texture               = wgpuDeviceCreateTexture(m_device, &depth_texture_desc);

    // Create the view of the depth texture manipulated by the rasterizer
    WGPUTextureViewDescriptor depth_texture_view_desc = {};
    depth_texture_view_desc.aspect                    = WGPUTextureAspect_DepthOnly;
    depth_texture_view_desc.baseArrayLayer            = 0;
    depth_texture_view_desc.arrayLayerCount           = 1;
    depth_texture_view_desc.baseMipLevel              = 0;
    depth_texture_view_desc.mipLevelCount             = 1;
    depth_texture_view_desc.dimension                 = WGPUTextureViewDimension_2D;
    depth_texture_view_desc.format                    = m_depth_texture_format;
    m_depth_texture_view                             = wgpuTextureCreateView(depth_texture, &depth_texture_view_desc);
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

    //auto on_device_error = [](WGPUErrorType type, WGPUStringView message, void* /* pUserData */, void*) {
    //    std::cout << "Uncaptured device error: type " << type;
    //    if (message.length > 0)
    //        std::cout << " (" << to_string(message) << ")";
    //    std::cout << std::endl;
    //};

    // wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
    wgpuAdapterRelease(adapter);
}

void Renderer::configure_surface() {
    m_swap_chain_format               = WGPUTextureFormat_BGRA8Unorm;
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

    configure_surface();
    initialize_depth_texture();
    initialize_gui();
}

void Renderer::on_camera_update() {
    for (auto& drawable : m_drawables) {
        drawable.second->on_camera_update();
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

} // namespace rr
