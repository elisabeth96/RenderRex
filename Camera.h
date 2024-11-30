#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/transform.hpp"

namespace rr {

class Camera {
    // We store the unmodified look at matrix along with
    // decomposed translation and rotation components
    glm::mat4 center_translation, translation;
    glm::quat rotation;
    // camera is the full camera transform,
    // inv_camera is stored as well to easily compute
    // eye position and world space rotation axes
    glm::mat4 camera, inv_camera;

public:
    Camera(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

    void rotate(glm::vec2 prev_mouse, glm::vec2 cur_mouse);

    void pan(glm::vec2 mouse_delta);

    void zoom(float zoom_amount);

    const glm::mat4& transform() const {
        return camera;
    }

    const glm::mat4& inv_transform() const {
        return inv_camera;
    }

    glm::vec3 eye() const;

    glm::vec3 dir() const;

    glm::vec3 up() const;

    glm::vec3 center() const;

private:
    void update_camera();
};

} // namespace rr
