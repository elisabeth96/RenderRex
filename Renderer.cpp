// contains all the unser interface funtions for the renderrex library

#include "Renderer.h"
#include "Drawable.h"
#include "glfw3webgpu/glfw3webgpu.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <iostream>
#include <webgpu/webgpu.h>

namespace rr {

/**
 * Utility function to get a WebGPU adapter
 */
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const* options) {
    // A simple structure holding the local information shared with the onAdapterRequestEnded
    // callback.
    struct UserData {
        WGPUAdapter adapter      = nullptr;
        bool        requestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message,
                                    void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cout << "Could not get WebGPU adapter: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    // Call to the WebGPU request adapter procedure
    wgpuInstanceRequestAdapter(instance, options, onAdapterRequestEnded, (void*)&userData);

    // We wait until userData.requestEnded gets true
#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

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
    viewDescriptor.label           = "Surface texture view";
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

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message,
                                   void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << message << std::endl;
        }
        userData.requestEnded = true;
    };

    wgpuAdapterRequestDevice(adapter, descriptor, onDeviceRequestEnded, (void*)&userData);

#ifdef __EMSCRIPTEN__
    while (!userData.requestEnded) {
        emscripten_sleep(100);
    }
#endif // __EMSCRIPTEN__

    assert(userData.requestEnded);

    return userData.device;
}
Renderer::~Renderer() { // release resources
    wgpuTextureViewRelease(m_depthTextureView);

    wgpuSwapChainRelease(m_swapChain);
    wgpuQueueRelease(m_queue);
    wgpuDeviceRelease(m_device);
    wgpuSurfaceUnconfigure(m_surface);
    wgpuSurfaceRelease(m_surface);
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

void Renderer::update_frame() {
    glfwPollEvents();

    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(m_swapChain);
    if (!nextTexture) {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        exit(1);
    }
    WGPUCommandEncoderDescriptor commandEncoderDesc = {};
    commandEncoderDesc.label                        = "Command Encoder";
    WGPUCommandEncoder encoder                      = wgpuDeviceCreateCommandEncoder(m_device, &commandEncoderDesc);

    WGPURenderPassEncoder renderPass = create_render_pass(nextTexture, encoder);

    for (auto& drawable : m_drawables) {
        drawable.second->draw(renderPass);
    }

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    wgpuTextureViewRelease(nextTexture);

    WGPUCommandBufferDescriptor cmdBufferDescriptor{};
    cmdBufferDescriptor.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    wgpuSwapChainPresent(m_swapChain);

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

void Renderer::register_mesh(std::string name, std::vector<glm::vec3>& positions,
                             std::vector<std::array<int, 3>>& triangles) {
    // create a unique pointer for the mesh
    std::unique_ptr<Mesh> mesh = std::make_unique<Mesh>(positions, triangles, *this);
    mesh->update_camera(m_camera, m_queue);
    m_drawables[name] = std::move(mesh);
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
    WGPUInstance instance = wgpuCreateInstance(&desc);

    // We can check whether there is actually an instance created
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        exit(EXIT_FAILURE);
    }

    m_surface = glfwGetWGPUSurface(instance, m_window);

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain               = nullptr;
    adapterOpts.compatibleSurface         = m_surface;
    WGPUAdapter adapter                   = requestAdapterSync(instance, &adapterOpts);
    wgpuInstanceRelease(instance);

    WGPUDeviceDescriptor deviceDesc     = {};
    deviceDesc.nextInChain              = nullptr;
    deviceDesc.label                    = "My Device"; // anything works here, that's your call
    deviceDesc.requiredFeatureCount     = 0;           // we do not require any specific feature
    deviceDesc.requiredLimits           = nullptr;     // we do not require any specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label       = "The default queue";
    deviceDesc.deviceLostCallback       = nullptr;

    m_device = requestDeviceSync(adapter, &deviceDesc);

    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
        std::cout << "Uncaptured device error: type " << type;
        if (message)
            std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /* pUserData */);
    wgpuAdapterRelease(adapter);
}

void Renderer::initialize_swap_chain() {
    // Create swap chain
    m_swapChainFormat                     = WGPUTextureFormat_BGRA8Unorm;
    WGPUSwapChainDescriptor swapChainDesc = {};
    swapChainDesc.width                   = m_width;
    swapChainDesc.height                  = m_height;
    swapChainDesc.usage                   = WGPUTextureUsage_RenderAttachment;
    swapChainDesc.format                  = m_swapChainFormat;
    swapChainDesc.presentMode             = WGPUPresentMode_Fifo;
    swapChainDesc.label                   = "Main swapchain";

    m_swapChain = wgpuDeviceCreateSwapChain(m_device, m_surface, &swapChainDesc);
    if (!m_swapChain) {
        std::cerr << "Failed to create swap chain" << std::endl;
        exit(1);
    }
}

void Renderer::initialize_queue() {
    m_queue = wgpuDeviceGetQueue(m_device);
    if (!m_queue) {
        std::cerr << "Failed to get device queue" << std::endl;
        exit(2);
    }
}

Renderer::Renderer() : m_camera({0, 0, 5}, {0, 0, 0}, {0, 1, 0}) {
    initialize_window();
    initialize_device();
    initialize_queue();
    initialize_swap_chain();
    initialize_depth_texture();
}

void Renderer::updateDrawableCameras() {
    for (auto& drawable : m_drawables) {
        drawable.second->update_camera(m_camera, m_queue);
    }
}

glm::vec2 transform_mouse(glm::vec2 in, uint32_t width, uint32_t height)
{
    return {in.x * 2.f / float(width) - 1.f, 1.f - 2.f * in.y / float(height)};
}

void Renderer::onMouseMove(double xpos, double ypos) {
    if (m_drag.active) {
        glm::vec2 current_pos = transform_mouse({xpos, ypos}, m_width, m_height);
        glm::vec2 last_pos = m_drag.last_pos;

        if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            m_camera.rotate(last_pos, current_pos);
        } else if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            glm::vec2 delta = (current_pos - last_pos) * m_drag.panSpeed;
            m_camera.pan(delta);
        }

        m_drag.last_pos = current_pos;

        updateDrawableCameras();
    }
}

void Renderer::onMouseButton(int button, int action, int /* modifiers */) {
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
    m_camera.zoom(static_cast<float>(yoffset) * m_drag.scrollSensitivity);
    updateDrawableCameras();
}

} // namespace rr
