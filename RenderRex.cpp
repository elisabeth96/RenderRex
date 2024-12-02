// contains all the unser interface funtions for the renderrex library

#include "RenderRex.h"
#include "Renderer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace rr {

void show() {
    Renderer& renderer = Renderer::get();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void* userData) {
            Renderer& renderer = *reinterpret_cast<Renderer*>(userData);
            renderer.update_frame();
        },
        (void*)&renderer, 0, true);
#else
    while (!renderer.should_close()) {
        renderer.update_frame();
    }
#endif
}

VisualMesh* register_mesh(std::string name, const std::vector<glm::vec3>& positions,
                          const std::vector<std::array<uint32_t, 3>>& triangles) {
    Mesh mesh(positions, triangles);
    return make_visual(name, mesh);
}

VisualMesh* make_visual(std::string name, const Mesh& mesh) {
    Renderer&   renderer = Renderer::get();
    Mesh        copy;
    const Mesh* m = &mesh;
    if (mesh.normal_faces.empty()) {
        copy = mesh;
        set_flat_normals(copy);
        m = &copy;
    }
    Drawable* drawable = renderer.register_drawable(name, std::make_unique<VisualMesh>(*m, renderer));
    return dynamic_cast<VisualMesh*>(drawable);
}

InstancedMesh* make_instanced(std::string name, const Mesh& mesh, size_t num_instances) {
    Renderer& renderer = Renderer::get();
    assert(!mesh.normal_faces.empty());
    Mesh        copy;
    Mesh const* mesh_ptr = &mesh;
    if (mesh.normal_faces.empty()) {
        copy = mesh;
        set_flat_normals(copy);
        mesh_ptr = &copy;
    }
    auto      im       = std::make_unique<InstancedMesh>(*mesh_ptr, num_instances, renderer);
    Drawable* drawable = renderer.register_drawable(name, std::move(im));
    return dynamic_cast<InstancedMesh*>(drawable);
}

void set_user_callback(std::function<void()> callback) {
    Renderer& renderer = Renderer::get();
    renderer.set_user_callback(callback);
}

} // namespace rr
