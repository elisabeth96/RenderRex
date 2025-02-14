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

// Utility function to get a WGPUStrinView from a std::string
/* WGPUStringView to_string_view(const std::string& str) {
    WGPUStringView view;
    view.data   = str.c_str();
    view.length = str.length();
    return view;
}*/

WGPUStringView to_string_view(const char* str) {
    WGPUStringView view;
    view.data   = str;
    view.length = strlen(str);
    return view;
}

std::string to_string(const WGPUStringView& view) {
    return std::string(view.data, view.length);
}

/*
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
    // A simple structure holding the local information shared with the onAdapterRequestEnded
    // callback.
    struct UserData {
        WGPUAdapter adapter      = nullptr;
        bool        requestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns
    WGPURequestAdapterCallback onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                                          WGPUStringView message, void* pUserData, void*) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::string str(message.data, message.data + message.length);
            std::cout << "Could not get WebGPU adapter: " << str << std::endl;
        }
        userData.requestEnded = true;
    };

    WGPURequestAdapterCallbackInfo callbackInfo = {nullptr, WGPUCallbackMode_WaitAnyOnly, onAdapterRequestEnded,
                                                   (void*)&userData, nullptr};

    // Call to the WebGPU request adapter procedure
    wgpuInstanceRequestAdapter(instance, options, callbackInfo);

    // We wait until userData.requestEnded gets true
#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    return userData.adapter;
}
*/

WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
    struct UserData {
        WGPUAdapter adapter      = nullptr;
        bool        requestEnded = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                                    void* pUserData, void*) {
        auto& ud = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            ud.adapter = adapter;
        } else {
            std::string str(message.data, message.data + message.length);
            std::cerr << "Could not get WebGPU adapter: " << str << std::endl;
        }
        ud.requestEnded = true;
    };

    // Important: Use AllowSpontaneous so Dawn can call back on its own thread.
    WGPURequestAdapterCallbackInfo callbackInfo = {/* userdataLabel  */ nullptr,
                                                   /* mode          */ WGPUCallbackMode_AllowSpontaneous,
                                                   /* callback      */ onAdapterRequestEnded,
                                                   /* userdata      */ &userData,
                                                   /* scope         */ nullptr};

    // Request the adapter
    wgpuInstanceRequestAdapter(instance, options, callbackInfo);

    // On native (non-Emscripten), we can just do a simple spin-wait
    // While Dawn runs the callback on an internal thread.
    while (!userData.requestEnded) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        // Alternatively, do your main loop tasks here.
    }

    // Now the callback should have set 'userData.adapter' or reported an error
    assert(userData.requestEnded);
    return userData.adapter;
}

/**
 * Utility function to get a WebGPU texture view from a surface
 */
WGPUTextureView GetNextSurfaceTextureView(WGPUSurface surface) {
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDescriptor;
    viewDescriptor.nextInChain     = nullptr;
    viewDescriptor.label           = to_string_view("Surface texture view");
    viewDescriptor.format          = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDescriptor.dimension       = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel    = 0;
    viewDescriptor.mipLevelCount   = 1;
    viewDescriptor.baseArrayLayer  = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect          = WGPUTextureAspect_All;

    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);
    return targetView;
}

/**
 * Utility function to get a WebGPU device. It is very similar to requestAdapter
 */
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const* descriptor) {
    struct UserData {
        WGPUDevice device       = nullptr;
        bool       requestEnded = false;
    };
    UserData userData;

    WGPURequestDeviceCallback onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                                        WGPUStringView message, void* pUserData, void*) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << to_string(message) << std::endl;
        }
        userData.requestEnded = true;
    };

    WGPURequestDeviceCallbackInfo callbackInfo = {nullptr, WGPUCallbackMode::WGPUCallbackMode_AllowSpontaneous,
                                                  onDeviceRequestEnded, (void*)&userData, nullptr};

    wgpuAdapterRequestDevice(adapter, descriptor, callbackInfo);

    while (!userData.requestEnded) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
        // Alternatively, do your main loop tasks here.
    }

    assert(userData.requestEnded);
    assert(userData.device);

    return userData.device;
}

void Renderer::terminate_gui() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}

Renderer::~Renderer() {
    // terminate GUI
    terminate_gui();

    // release resources
    wgpuTextureViewRelease(m_depthTextureView);

    wgpuQueueRelease(m_queue);
    wgpuDeviceRelease(m_device);
    wgpuSurfaceUnconfigure(m_surface);
    wgpuSurfaceRelease(m_surface);
    wgpuInstanceRelease(m_instance);

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

WGPURenderPassEncoder Renderer::create_render_pass(WGPUTextureView nextTexture, WGPUCommandEncoder encoder) {

    WGPURenderPassDescriptor renderPassDesc{};

    WGPURenderPassColorAttachment renderPassColorAttachment{};
    renderPassColorAttachment.view          = nextTexture;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp        = WGPULoadOp_Clear;
    renderPassColorAttachment.storeOp       = WGPUStoreOp_Store;
    renderPassColorAttachment.clearValue    = WGPUColor{0.4, 0.4, 1, 1};
    renderPassColorAttachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;
    renderPassColorAttachment.nextInChain   = nullptr;
    renderPassDesc.colorAttachmentCount     = 1;
    renderPassDesc.colorAttachments         = &renderPassColorAttachment;
    renderPassDesc.nextInChain              = nullptr;

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view                                 = m_depthTextureView;
    depthStencilAttachment.depthClearValue                      = 1.0f;
    depthStencilAttachment.depthLoadOp                          = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp                         = WGPUStoreOp_Store;
    depthStencilAttachment.depthReadOnly                        = false;
    depthStencilAttachment.stencilClearValue                    = 0;
#ifdef WEBGPU_BACKEND_WGPU
    depthStencilAttachment.stencilLoadOp  = LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
    depthStencilAttachment.stencilLoadOp  = WGPULoadOp_Undefined;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
#endif
    depthStencilAttachment.stencilReadOnly = true;

    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    // renderPassDesc.timestampWriteCount = 0;
    renderPassDesc.timestampWrites = nullptr;
    return wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
}

void Renderer::update_gui(WGPURenderPassEncoder renderPass) {
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
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}

void Renderer::update_frame() {
    glfwPollEvents();

    if (m_user_callback) {
        m_user_callback();
    }
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.nextInChain               = nullptr;
    viewDesc.label                     = to_string_view("Surface texture view");
    viewDesc.format                    = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDesc.dimension                 = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel              = 0;
    viewDesc.mipLevelCount             = 1;
    viewDesc.baseArrayLayer            = 0;
    viewDesc.arrayLayerCount           = 1;
    viewDesc.aspect                    = WGPUTextureAspect_All;

    WGPUTextureView nextTexture = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        exit(1);
    }
    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.label                        = to_string_view("Command Encoder");
    WGPUCommandEncoder encoder                      = wgpuDeviceCreateCommandEncoder(m_device, &commandEncoderDesc);

    WGPURenderPassEncoder renderPass = create_render_pass(nextTexture, encoder);

    for (auto& drawable : m_drawables) {
        drawable.second->draw(renderPass);
    }

    // We add the GUI drawing commands to the render pass
    update_gui(renderPass);

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    wgpuTextureViewRelease(nextTexture);

    WGPUCommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = to_string_view("Command buffer");
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuSurfacePresent(m_surface);
    wgpuTextureRelease(surfaceTexture.texture);

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
    m_depthTextureFormat                   = WGPUTextureFormat_Depth24Plus;
    WGPUTextureDescriptor depthTextureDesc = {};
    depthTextureDesc.dimension             = WGPUTextureDimension_2D;
    depthTextureDesc.format                = m_depthTextureFormat;
    depthTextureDesc.mipLevelCount         = 1;
    depthTextureDesc.sampleCount           = 1;
    depthTextureDesc.size                  = {m_width, m_height, 1};
    depthTextureDesc.usage                 = WGPUTextureUsage_RenderAttachment;
    depthTextureDesc.viewFormatCount       = 1;
    depthTextureDesc.viewFormats           = (WGPUTextureFormat*)&m_depthTextureFormat;
    WGPUTexture depthTexture               = wgpuDeviceCreateTexture(m_device, &depthTextureDesc);

    // Create the view of the depth texture manipulated by the rasterizer
    WGPUTextureViewDescriptor depthTextureViewDesc = {};
    depthTextureViewDesc.aspect                    = WGPUTextureAspect_DepthOnly;
    depthTextureViewDesc.baseArrayLayer            = 0;
    depthTextureViewDesc.arrayLayerCount           = 1;
    depthTextureViewDesc.baseMipLevel              = 0;
    depthTextureViewDesc.mipLevelCount             = 1;
    depthTextureViewDesc.dimension                 = WGPUTextureViewDimension_2D;
    depthTextureViewDesc.format                    = m_depthTextureFormat;
    m_depthTextureView                             = wgpuTextureCreateView(depthTexture, &depthTextureViewDesc);
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
            that->onMouseMove(xpos, ypos);
    });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr)
            that->onMouseButton(button, action, mods);
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto that = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        if (that != nullptr)
            that->onScroll(xoffset, yoffset);
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
    WGPUAdapter adapter                   = requestAdapterSync(m_instance, &adapterOpts);

    WGPUDeviceDescriptor deviceDesc     = {};
    deviceDesc.nextInChain              = nullptr;
    deviceDesc.label                    = to_string_view("My Device"); // anything works here, that's your call
    deviceDesc.requiredFeatureCount     = 0;                           // we do not require any specific feature
    deviceDesc.requiredLimits           = nullptr;                     // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label       = to_string_view("The default queue");
    // deviceDesc.deviceLostCallback       = nullptr;

    m_device = requestDeviceSync(adapter, &deviceDesc);

    auto onDeviceError = [](WGPUErrorType type, WGPUStringView message, void* /* pUserData */, void*) {
        std::cout << "Uncaptured device error: type " << type;
        if (message.length > 0)
            std::cout << " (" << to_string(message) << ")";
        std::cout << std::endl;
    };

    // wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
    wgpuAdapterRelease(adapter);
}

void Renderer::configure_surface() {
    m_swapChainFormat               = WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration config = {};
    config.device                   = m_device;
    config.format                   = m_swapChainFormat;
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
    init_info.RenderTargetFormat      = m_swapChainFormat;
    init_info.DepthStencilFormat      = m_depthTextureFormat;
    ImGui_ImplWGPU_Init(&init_info);
}

Renderer::Renderer() : m_camera({0, 0, 5}, {0, 0, 0}, {0, 1, 0}) {
    initialize_window();
    initialize_device();
    initialize_queue();
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

void Renderer::onMouseMove(double xpos, double ypos) {
    // If imgui wants the mouse, we don't want to interfere
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    if (m_drag.active) {
        glm::vec2 current_pos = transform_mouse({xpos, ypos}, m_width, m_height);
        glm::vec2 last_pos    = m_drag.last_pos;

        if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            m_camera.rotate(last_pos, current_pos);
        } else if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            glm::vec2 delta = (current_pos - last_pos) * m_drag.panSpeed;
            m_camera.pan(delta);
        }

        m_drag.last_pos = current_pos;

        on_camera_update();
    }
}

void Renderer::onMouseButton(int button, int action, int /* modifiers */) {
    // If imgui wants the mouse, we don't want to interfere
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

void Renderer::onScroll(double /* xoffset */, double yoffset) {
    // If imgui wants the mouse, we don't want to interfere
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }
    m_camera.zoom(static_cast<float>(yoffset) * m_drag.scrollSensitivity);
    on_camera_update();
}

void Renderer::resize(int width, int height) {
    m_width  = width;
    m_height = height;

    // Reconfigure the swap chain with the new dimensions.
    configure_surface();

    // Release the old depth texture view.
    if (m_depthTextureView) {
        wgpuTextureViewRelease(m_depthTextureView);
    }

    // Recreate the depth texture for the new size.
    initialize_depth_texture();

    on_camera_update();
}

} // namespace rr
