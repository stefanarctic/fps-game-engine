#pragma once
#include "Forge/Core/Json.h"
#include "Forge/ECS/Reflection.h"
#include "Forge/Renderer/Mesh.h"
#include "Forge/Renderer/Texture.h"

namespace Forge {

enum class ShaderModel : int {
    Lit = 0,    // physically based (metallic/roughness), shadows, SSAO, fog
    Unlit = 1,  // flat color * texture, ignores lighting
};

// Material asset (.mat JSON) or a material embedded in a model file.
class Material {
public:
    std::string name = "Material";
    std::string path;                 // asset path when loaded from a .mat file

    ShaderModel shader = ShaderModel::Lit;
    glm::vec4 albedo{1.0f};
    std::string albedoMap;            // texture asset path
    float metallic = 0.0f;
    float roughness = 0.6f;
    glm::vec3 emission{0.0f};         // HDR, added after lighting
    glm::vec2 tiling{1.0f};
    bool worldSpaceUV = false;        // triplanar mapping in world units (great for level blockouts)

    // Runtime
    Ref<Texture2D> albedoTexture;     // resolved from albedoMap, or embedded texture
    std::string loadedAlbedoMap;

    Json toJson() const;
    void fromJson(const Json& json);

    static TypeMeta& meta();
};

struct SubMesh {
    Ref<Mesh> mesh;
    uint32_t materialIndex = 0;
};

// A loaded model file (OBJ / glTF) or a built-in primitive.
class Model {
public:
    std::string path;
    std::vector<SubMesh> submeshes;
    std::vector<Ref<Material>> materials;
    AABB bounds;

    void computeBounds() {
        bounds = AABB();
        for (const SubMesh& s : submeshes) bounds.expand(s.mesh->bounds());
    }
    // Closest ray hit in model space.
    bool raycast(const Ray& localRay, float& t) const {
        bool hit = false;
        float best = FLT_MAX;
        for (const SubMesh& s : submeshes) {
            float st;
            if (s.mesh->raycast(localRay, st) && st < best) { best = st; hit = true; }
        }
        if (hit) t = best;
        return hit;
    }
};

} // namespace Forge
