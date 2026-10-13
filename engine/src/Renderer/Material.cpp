#include "Forge/Renderer/Material.h"

namespace Forge {

TypeMeta& Material::meta() {
    static TypeMeta meta;
    static bool init = false;
    if (!init) {
        init = true;
        describe<Material>(meta, "Material")
            .enumeration("Shader", &Material::shader, {"Lit", "Unlit"})
            .color("Albedo", &Material::albedo)
            .asset("Albedo Map", &Material::albedoMap, AssetKind::Texture)
            .field("Metallic", &Material::metallic, 0.01f, 0.0f, 1.0f)
            .field("Roughness", &Material::roughness, 0.01f, 0.02f, 1.0f)
            .color("Emission", &Material::emission, true)
            .field("Tiling", &Material::tiling, 0.01f)
            .field("World Space UV", &Material::worldSpaceUV).tooltip("Triplanar projection; Tiling = repeats per meter");
    }
    return meta;
}

Json Material::toJson() const {
    Json j = Reflection::toJson(meta(), this);
    j["Name"] = Json(name);
    return j;
}

void Material::fromJson(const Json& json) {
    Reflection::fromJson(meta(), this, json);
    name = json["Name"].asString(name);
}

} // namespace Forge
