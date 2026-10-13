#pragma once
#include "Forge/Renderer/Mesh.h"

namespace Forge {

// Built-in meshes with Unity-compatible dimensions.
namespace Primitives {
Ref<Mesh> cube();                       // 1x1x1
Ref<Mesh> sphere(int segments = 32, int rings = 16); // diameter 1
Ref<Mesh> plane(int subdivisions = 10); // 10x10 on XZ, facing +Y
Ref<Mesh> quad();                       // 1x1 on XY, facing +Z
Ref<Mesh> cylinder(int segments = 32);  // height 2, diameter 1
Ref<Mesh> capsule(int segments = 32, int rings = 8); // height 2, diameter 1
} // namespace Primitives

} // namespace Forge
