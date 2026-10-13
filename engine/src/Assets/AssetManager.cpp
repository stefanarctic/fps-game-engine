#include "Forge/Assets/AssetManager.h"
#include "Forge/Assets/ModelLoaders.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"
#include "Forge/Renderer/Primitives.h"

#include <unordered_map>

namespace Forge {

namespace {
struct Cache {
    std::unordered_map<std::string, Ref<Model>> models;
    std::unordered_map<std::string, Ref<Material>> materials;
    std::unordered_map<std::string, Ref<Texture2D>> textures;
    std::unordered_map<std::string, Ref<Shader>> shaders;
    std::unordered_map<std::string, bool> failed; // avoid re-trying (and re-logging) every frame
    Ref<Material> defaultMaterial;
    Ref<Texture2D> white;
};

Cache& C() {
    static Cache c;
    return c;
}

Ref<Model> makeBuiltin(const std::string& name) {
    Ref<Mesh> mesh;
    if (name == "Cube") mesh = Primitives::cube();
    else if (name == "Sphere") mesh = Primitives::sphere();
    else if (name == "Plane") mesh = Primitives::plane();
    else if (name == "Quad") mesh = Primitives::quad();
    else if (name == "Cylinder") mesh = Primitives::cylinder();
    else if (name == "Capsule") mesh = Primitives::capsule();
    if (!mesh) return nullptr;
    auto model = MakeRef<Model>();
    model->path = "builtin:" + name;
    model->materials.push_back(AssetManager::defaultMaterial());
    model->submeshes.push_back({mesh, 0});
    model->computeBounds();
    return model;
}
} // namespace

const std::vector<std::string>& AssetManager::builtinMeshes() {
    static const std::vector<std::string> names = {"builtin:Cube", "builtin:Sphere", "builtin:Plane",
                                                   "builtin:Quad", "builtin:Cylinder", "builtin:Capsule"};
    return names;
}

Ref<Texture2D> AssetManager::whiteTexture() {
    if (!C().white) {
        const uint8_t white[4] = {255, 255, 255, 255};
        C().white = Texture2D::fromRGBA(1, 1, white, false, false, true);
    }
    return C().white;
}

Ref<Material> AssetManager::defaultMaterial() {
    if (!C().defaultMaterial) {
        auto m = MakeRef<Material>();
        m->name = "Default-Material";
        m->albedo = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        m->roughness = 0.6f;
        C().defaultMaterial = m;
    }
    return C().defaultMaterial;
}

Ref<Model> AssetManager::getModel(const std::string& path) {
    if (path.empty()) return nullptr;
    Cache& c = C();
    auto it = c.models.find(path);
    if (it != c.models.end()) return it->second;
    if (c.failed.count(path)) return nullptr;

    Ref<Model> model;
    if (path.rfind("builtin:", 0) == 0) {
        model = makeBuiltin(path.substr(8));
    } else {
        std::string ext = FileSystem::extension(path);
        std::string abs = FileSystem::resolve(path);
        if (ext == ".obj") model = loadOBJ(abs);
        else if (ext == ".gltf" || ext == ".glb") model = loadGLTF(abs);
        else LOG_ERROR("Unsupported model format '%s'", path.c_str());
    }
    if (!model) {
        c.failed[path] = true;
        return nullptr;
    }
    for (auto& m : model->materials) resolveTextures(*m);
    c.models[path] = model;
    return model;
}

Ref<Material> AssetManager::getMaterial(const std::string& path) {
    if (path.empty()) return nullptr;
    Cache& c = C();
    auto it = c.materials.find(path);
    if (it != c.materials.end()) return it->second;
    if (c.failed.count(path)) return nullptr;

    std::string text;
    Json json;
    std::string error;
    if (!FileSystem::readText(FileSystem::resolve(path), text) || !Json::parse(text, json, &error)) {
        LOG_ERROR("Failed to load material '%s' %s", path.c_str(), error.c_str());
        c.failed[path] = true;
        return nullptr;
    }
    auto material = MakeRef<Material>();
    material->name = FileSystem::stem(path);
    material->fromJson(json);
    material->path = path;
    resolveTextures(*material);
    c.materials[path] = material;
    return material;
}

Ref<Texture2D> AssetManager::getTexture(const std::string& path, bool srgb) {
    if (path.empty()) return nullptr;
    Cache& c = C();
    std::string key = path + (srgb ? "|srgb" : "|linear");
    auto it = c.textures.find(key);
    if (it != c.textures.end()) return it->second;
    if (c.failed.count(key)) return nullptr;
    Ref<Texture2D> tex = Texture2D::loadFromFile(FileSystem::resolve(path), srgb);
    if (!tex) {
        c.failed[key] = true;
        return nullptr;
    }
    tex->path = path;
    c.textures[key] = tex;
    return tex;
}

void AssetManager::resolveTextures(Material& material) {
    if (material.albedoMap == material.loadedAlbedoMap) return;
    material.loadedAlbedoMap = material.albedoMap;
    material.albedoTexture = material.albedoMap.empty() ? nullptr : getTexture(material.albedoMap, true);
}

Ref<Shader> AssetManager::getShader(const std::string& name) {
    Cache& c = C();
    auto it = c.shaders.find(name);
    if (it != c.shaders.end()) return it->second;
    Ref<Shader> shader = Shader::fromFile(FileSystem::resolve("shaders/" + name + ".glsl"));
    if (!shader) LOG_ERROR("Shader '%s' failed to load", name.c_str());
    c.shaders[name] = shader; // cache failures too (nullptr) to avoid recompiling every frame
    return shader;
}

int AssetManager::reloadShaders(bool force) {
    int reloaded = 0;
    for (auto& kv : C().shaders) {
        if (!kv.second) {
            // Retry shaders that failed to compile before.
            kv.second = Shader::fromFile(FileSystem::resolve("shaders/" + kv.first + ".glsl"));
            if (kv.second) reloaded++;
            continue;
        }
        if (force || FileSystem::lastWriteTime(kv.second->path()) != kv.second->fileTime()) {
            if (kv.second->reload()) {
                reloaded++;
                LOG_INFO("Reloaded shader '%s'", kv.first.c_str());
            }
        }
    }
    return reloaded;
}

bool AssetManager::saveMaterial(const Ref<Material>& material, const std::string& path) {
    if (!material) return false;
    if (!FileSystem::writeText(FileSystem::resolve(path), material->toJson().dump(2))) {
        LOG_ERROR("Failed to save material '%s'", path.c_str());
        return false;
    }
    material->path = path;
    C().materials[path] = material;
    LOG_INFO("Saved material '%s'", path.c_str());
    return true;
}

Ref<Material> AssetManager::createMaterial(const std::string& path) {
    auto material = MakeRef<Material>();
    material->name = FileSystem::stem(path);
    if (!saveMaterial(material, path)) return nullptr;
    return material;
}

void AssetManager::reloadAssets() {
    Cache& c = C();
    c.models.clear();
    c.materials.clear();
    c.textures.clear();
    c.failed.clear();
    LOG_INFO("Asset caches cleared; assets will reload on demand");
}

void AssetManager::shutdown() {
    Cache& c = C();
    c.models.clear();
    c.materials.clear();
    c.textures.clear();
    c.shaders.clear();
    c.failed.clear();
    c.defaultMaterial.reset();
    c.white.reset();
}

size_t AssetManager::loadedModelCount() { return C().models.size(); }

} // namespace Forge
