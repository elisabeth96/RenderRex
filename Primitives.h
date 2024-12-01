#pragma once

#include "Mesh.h"

namespace rr {

Mesh create_box();

Mesh create_sphere(size_t latitudes = 16, size_t longitudes = 32);

Mesh create_cylinder(size_t segments = 16);

Mesh create_cone(size_t segments = 16);

}