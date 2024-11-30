#pragma once

#include "glm/glm.hpp"
#include <vector>

namespace rr {

struct BoundingBox {
    BoundingBox() : lower(glm::vec3(FLT_MAX)), upper(glm::vec3(-FLT_MAX)) {}

    explicit BoundingBox(const std::vector<glm::vec3>& pts) {
        lower = glm::vec3(FLT_MAX);
        upper = glm::vec3(-FLT_MAX);
        for(const glm::vec3& pt : pts) {
            lower = glm::min(lower, pt);
            upper = glm::max(upper, pt);
        }
    }

    void expand_to_include(const BoundingBox& other) {
        lower = glm::min(lower, other.lower);
        upper = glm::max(upper, other.upper);
    }

    glm::vec3 lower;
    glm::vec3 upper;
};


}