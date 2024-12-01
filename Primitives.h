#pragma once

#include "Mesh.h"

namespace rr {

Mesh create_box();

Mesh create_sphere(float radius = 1.0f, int latitudes = 16, int longitudes = 32);


}