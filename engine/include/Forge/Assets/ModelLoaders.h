#pragma once
#include "Forge/Renderer/Material.h"

namespace Forge {

// Wavefront OBJ + MTL loader.
//  - v / vt / vn, polygon faces (fan triangulated), negative indices
//  - o / g / usemtl split the model into one sub-mesh per material
//  - MTL: Kd, Ke, Ns (-> roughness), Pm/Pr (PBR extension), d, map_Kd
//  - smooth normals are generated when the file has none
Ref<Model> loadOBJ(const std::string& absolutePath);

// glTF 2.0 loader (.gltf with external/base64 buffers, or binary .glb).
//  - every mesh primitive in the default scene, node transforms baked in
//  - POSITION / NORMAL / TEXCOORD_0, any index component type
//  - pbrMetallicRoughness: baseColorFactor/Texture, metallic, roughness, emissive
//  - textures from files, data URIs or GLB buffer views (PNG/JPG)
Ref<Model> loadGLTF(const std::string& absolutePath);

} // namespace Forge
