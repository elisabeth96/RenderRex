#include "Camera.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

namespace rr {
glm::quat screen_to_arcball(const glm::vec2& p) {
    const float dist = glm::length2(p); // glm::length2 computes the squared length.

    // If we're on/in the sphere, return the point on it
    if (dist <= 1.0f) {
        return {0.0f, p.x, p.y, std::sqrt(1.0f - dist)};
    } else {
        // Otherwise, we project the point onto the sphere
        const glm::vec2 proj = glm::normalize(p);
        return {0.0f, proj.x, proj.y, 0.0f};
    }
}

// Constructor
Camera::Camera(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
    glm::vec3 dir    = center - eye;
    glm::vec3 z_axis = glm::normalize(dir);
    glm::vec3 x_axis = glm::normalize(glm::cross(z_axis, glm::normalize(up)));
    glm::vec3 y_axis = glm::normalize(glm::cross(x_axis, z_axis));
    x_axis           = glm::normalize(glm::cross(z_axis, y_axis));

    center_translation = glm::inverse(glm::translate(glm::mat4(1.0f), center));
    translation        = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -glm::length(dir)));
    rotation           = glm::quat_cast(glm::mat3(x_axis, y_axis, -z_axis));

    update_camera();
}

void Camera::rotate(glm::vec2 prev_mouse, glm::vec2 cur_mouse) {
    // Clamp mouse positions to stay in NDC
    prev_mouse = glm::clamp(prev_mouse, glm::vec2(-1, -1), glm::vec2(1, 1));
    cur_mouse  = glm::clamp(cur_mouse, glm::vec2(-1, -1), glm::vec2(1, 1));

    glm::quat mouse_cur_ball  = screen_to_arcball(cur_mouse);
    glm::quat mouse_prev_ball = screen_to_arcball(prev_mouse);

    rotation = mouse_cur_ball * mouse_prev_ball * rotation;

    update_camera();
}

void Camera::pan(glm::vec2 mouse_delta) {
    float     zoom_amount = glm::abs(translation[3][2]);
    glm::vec4 motion(mouse_delta.x * zoom_amount, mouse_delta.y * zoom_amount, 0.0f, 0.0f);
    // Find the panning amount in the world space
    motion = inv_camera * motion;

    center_translation = glm::translate(glm::vec3(motion)) * center_translation;
    update_camera();
}

void Camera::zoom(float zoom_amount) {
    glm::vec3 motion(0.0f, 0.0f, zoom_amount);
    translation = glm::translate(motion) * translation;
    update_camera();
}

glm::vec3 Camera::eye() const {
    return inv_camera * glm::vec4(0, 0, 0, 1);
}

glm::vec3 Camera::dir() const {
    return glm::normalize(glm::vec3(inv_camera * glm::vec4(0, 0, -1, 0)));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::vec3(inv_camera * glm::vec4(0, 1, 0, 0)));
}

void Camera::update_camera() {
    camera     = translation * glm::toMat4(rotation) * center_translation;
    inv_camera = glm::inverse(camera);
}
} // namespace rr
