#pragma once
#include "Forge/Renderer/Material.h"
#include "Forge/Renderer/Shader.h"

namespace Forge {

// Central cache for every loadable resource. Paths are asset paths relative to
// the asset root ("models/crate.obj"). Built-in meshes use "builtin:<Name>".
class AssetManager {
public:
    static Ref<Model> getModel(const std::string& path);
    static Ref<Material> getMaterial(const std::string& path);   // .mat file
    static Ref<Texture2D> getTexture(const std::string& path, bool srgb = true);
    static Ref<Shader> getShader(const std::string& name);       // "Lit" -> shaders/Lit.glsl

    static Ref<Material> defaultMaterial();
    static Ref<Texture2D> whiteTexture();
    static const std::vector<std::string>& builtinMeshes();

    // Makes sure the material's albedo texture matches its albedoMap path.
    static void resolveTextures(Material& material);

    static bool saveMaterial(const Ref<Material>& material, const std::string& path);
    static Ref<Material> createMaterial(const std::string& path); // new .mat file on disk

    // Recompiles every shader whose file changed (or all when force is set).
    static int reloadShaders(bool force = false);
    // Drops cached models / materials / textures so they reload from disk.
    static void reloadAssets();
    static void shutdown();

    static size_t loadedModelCount();
};

} // namespace Forge
