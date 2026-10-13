#include "Forge/Assets/ModelLoaders.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Json.h"
#include "Forge/Core/Log.h"

#include <cstring>
#include <unordered_map>

namespace Forge {

namespace {

// glTF component types
constexpr int kByte = 5120, kUByte = 5121, kShort = 5122, kUShort = 5123, kUInt = 5125, kFloat = 5126;

int componentSize(int type) {
    switch (type) {
    case kByte:
    case kUByte: return 1;
    case kShort:
    case kUShort: return 2;
    case kUInt:
    case kFloat: return 4;
    }
    return 0;
}

int componentCount(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT2") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 0;
}

bool decodeBase64(const std::string& in, size_t start, std::vector<uint8_t>& out) {
    static int table[256];
    static bool init = false;
    if (!init) {
        for (int& t : table) t = -1;
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) table[(unsigned char)chars[i]] = i;
        init = true;
    }
    out.clear();
    out.reserve((in.size() - start) * 3 / 4);
    uint32_t buffer = 0;
    int bits = 0;
    for (size_t i = start; i < in.size(); ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=') break;
        int v = table[c];
        if (v < 0) continue; // skip whitespace / invalid
        buffer = (buffer << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((buffer >> bits) & 0xFF));
        }
    }
    return true;
}

struct GltfContext {
    std::string path;
    std::string dir;
    Json doc;
    const Json& cdoc() const { return doc; } // const access never inserts keys
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<uint8_t> glbBin;
    std::unordered_map<int, Ref<Texture2D>> textures; // image index -> texture
};

bool loadUri(GltfContext& ctx, const std::string& uri, std::vector<uint8_t>& out) {
    if (uri.rfind("data:", 0) == 0) {
        size_t comma = uri.find(',');
        if (comma == std::string::npos || uri.find(";base64") == std::string::npos) return false;
        return decodeBase64(uri, comma + 1, out);
    }
    // Percent-decode spaces (%20) which are common in exported file names.
    std::string file;
    for (size_t i = 0; i < uri.size(); ++i) {
        if (uri[i] == '%' && i + 2 < uri.size()) {
            file += (char)std::strtol(uri.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else {
            file += uri[i];
        }
    }
    return FileSystem::readBinary(FileSystem::join(ctx.dir, file), out);
}

bool loadBuffers(GltfContext& ctx) {
    const Json& buffers = ctx.cdoc()["buffers"];
    for (size_t i = 0; i < buffers.size(); ++i) {
        const Json& b = buffers[i];
        std::vector<uint8_t> data;
        if (b.has("uri")) {
            if (!loadUri(ctx, b["uri"].asString(), data)) {
                LOG_ERROR("glTF '%s': cannot load buffer %zu", ctx.path.c_str(), i);
                return false;
            }
        } else if (i == 0) {
            data = ctx.glbBin; // GLB binary chunk
        }
        ctx.buffers.push_back(std::move(data));
    }
    return true;
}

// Reads an accessor into floats (normalized integer types are converted to [0,1] / [-1,1]).
bool readAccessor(GltfContext& ctx, int accessorIndex, int expectedComponents, std::vector<float>& out) {
    const Json& acc = ctx.cdoc()["accessors"][(size_t)accessorIndex];
    if (!acc.isObject()) return false;
    int count = acc["count"].asInt();
    int comps = componentCount(acc["type"].asString());
    int ctype = acc["componentType"].asInt();
    bool normalized = acc["normalized"].asBool(false);
    if (comps != expectedComponents || componentSize(ctype) == 0) return false;

    out.assign((size_t)count * (size_t)comps, 0.0f);
    if (!acc.has("bufferView")) return true; // all zeros (sparse not supported)

    const Json& view = ctx.cdoc()["bufferViews"][(size_t)acc["bufferView"].asInt()];
    int bufferIndex = view["buffer"].asInt();
    if (bufferIndex < 0 || bufferIndex >= (int)ctx.buffers.size()) return false;
    const std::vector<uint8_t>& buffer = ctx.buffers[(size_t)bufferIndex];
    size_t offset = (size_t)view["byteOffset"].asInt(0) + (size_t)acc["byteOffset"].asInt(0);
    size_t elemSize = (size_t)componentSize(ctype) * (size_t)comps;
    size_t stride = (size_t)view["byteStride"].asInt(0);
    if (stride == 0) stride = elemSize;
    if (count > 0 && offset + stride * (size_t)(count - 1) + elemSize > buffer.size()) {
        LOG_ERROR("glTF '%s': accessor %d out of buffer range", ctx.path.c_str(), accessorIndex);
        return false;
    }

    for (int i = 0; i < count; ++i) {
        const uint8_t* src = buffer.data() + offset + stride * (size_t)i;
        for (int c = 0; c < comps; ++c) {
            float v = 0.0f;
            switch (ctype) {
            case kFloat: { float f; std::memcpy(&f, src + c * 4, 4); v = f; break; }
            case kUByte: { uint8_t x = src[c]; v = normalized ? x / 255.0f : (float)x; break; }
            case kByte: { int8_t x; std::memcpy(&x, src + c, 1); v = normalized ? glm::max(x / 127.0f, -1.0f) : (float)x; break; }
            case kUShort: { uint16_t x; std::memcpy(&x, src + c * 2, 2); v = normalized ? x / 65535.0f : (float)x; break; }
            case kShort: { int16_t x; std::memcpy(&x, src + c * 2, 2); v = normalized ? glm::max(x / 32767.0f, -1.0f) : (float)x; break; }
            case kUInt: { uint32_t x; std::memcpy(&x, src + c * 4, 4); v = (float)x; break; }
            }
            out[(size_t)i * comps + c] = v;
        }
    }
    return true;
}

bool readIndices(GltfContext& ctx, int accessorIndex, std::vector<uint32_t>& out) {
    const Json& acc = ctx.cdoc()["accessors"][(size_t)accessorIndex];
    int count = acc["count"].asInt();
    int ctype = acc["componentType"].asInt();
    if (!acc.has("bufferView")) return false;
    const Json& view = ctx.cdoc()["bufferViews"][(size_t)acc["bufferView"].asInt()];
    const std::vector<uint8_t>& buffer = ctx.buffers[(size_t)view["buffer"].asInt()];
    size_t offset = (size_t)view["byteOffset"].asInt(0) + (size_t)acc["byteOffset"].asInt(0);
    size_t size = (size_t)componentSize(ctype);
    size_t stride = (size_t)view["byteStride"].asInt(0);
    if (stride == 0) stride = size;
    if (size == 0 || (count > 0 && offset + stride * (size_t)(count - 1) + size > buffer.size())) return false;
    out.resize((size_t)count);
    for (int i = 0; i < count; ++i) {
        const uint8_t* src = buffer.data() + offset + stride * (size_t)i;
        if (ctype == kUByte) out[(size_t)i] = src[0];
        else if (ctype == kUShort) { uint16_t x; std::memcpy(&x, src, 2); out[(size_t)i] = x; }
        else { uint32_t x; std::memcpy(&x, src, 4); out[(size_t)i] = x; }
    }
    return true;
}

Ref<Texture2D> loadImage(GltfContext& ctx, int textureIndex) {
    const Json& tex = ctx.cdoc()["textures"][(size_t)textureIndex];
    if (!tex.has("source")) return nullptr;
    int imageIndex = tex["source"].asInt();
    auto cached = ctx.textures.find(imageIndex);
    if (cached != ctx.textures.end()) return cached->second;

    const Json& image = ctx.cdoc()["images"][(size_t)imageIndex];
    std::vector<uint8_t> bytes;
    std::string name = ctx.path + "#image" + std::to_string(imageIndex);
    if (image.has("uri")) {
        if (!loadUri(ctx, image["uri"].asString(), bytes)) {
            LOG_WARN("glTF '%s': cannot load image %d", ctx.path.c_str(), imageIndex);
            return nullptr;
        }
    } else if (image.has("bufferView")) {
        const Json& view = ctx.cdoc()["bufferViews"][(size_t)image["bufferView"].asInt()];
        const std::vector<uint8_t>& buffer = ctx.buffers[(size_t)view["buffer"].asInt()];
        size_t offset = (size_t)view["byteOffset"].asInt(0);
        size_t length = (size_t)view["byteLength"].asInt(0);
        if (offset + length > buffer.size()) return nullptr;
        bytes.assign(buffer.begin() + (std::ptrdiff_t)offset, buffer.begin() + (std::ptrdiff_t)(offset + length));
    }
    Ref<Texture2D> texture = Texture2D::loadFromMemory(bytes.data(), bytes.size(), true, name);
    ctx.textures[imageIndex] = texture;
    return texture;
}

glm::mat4 nodeLocalMatrix(const Json& node) {
    if (node.has("matrix")) {
        const Json& m = node["matrix"];
        glm::mat4 r(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) r[c][rr] = m[(size_t)(c * 4 + rr)].asFloat(c == rr ? 1.0f : 0.0f);
        return r;
    }
    glm::vec3 t(0.0f), s(1.0f);
    glm::quat q(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.has("translation")) {
        const Json& a = node["translation"];
        t = glm::vec3(a[0].asFloat(), a[1].asFloat(), a[2].asFloat());
    }
    if (node.has("rotation")) {
        const Json& a = node["rotation"]; // x, y, z, w
        q = glm::quat(a[3].asFloat(1.0f), a[0].asFloat(), a[1].asFloat(), a[2].asFloat());
    }
    if (node.has("scale")) {
        const Json& a = node["scale"];
        s = glm::vec3(a[0].asFloat(1.0f), a[1].asFloat(1.0f), a[2].asFloat(1.0f));
    }
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
}

struct Batch {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

void appendMesh(GltfContext& ctx, int meshIndex, const glm::mat4& world, std::unordered_map<int, Batch>& batches, int defaultMaterial) {
    const Json& mesh = ctx.cdoc()["meshes"][(size_t)meshIndex];
    const Json& prims = mesh["primitives"];
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(world)));
    bool flipWinding = glm::determinant(glm::mat3(world)) < 0.0f;

    for (size_t p = 0; p < prims.size(); ++p) {
        const Json& prim = prims[p];
        int mode = prim["mode"].asInt(4);
        if (mode != 4) {
            LOG_WARN("glTF '%s': skipping non-triangle primitive (mode %d)", ctx.path.c_str(), mode);
            continue;
        }
        const Json& attrs = prim["attributes"];
        if (!attrs.has("POSITION")) continue;

        std::vector<float> pos, nrm, uv;
        if (!readAccessor(ctx, attrs["POSITION"].asInt(), 3, pos)) continue;
        bool hasNormals = attrs.has("NORMAL") && readAccessor(ctx, attrs["NORMAL"].asInt(), 3, nrm);
        bool hasUVs = attrs.has("TEXCOORD_0") && readAccessor(ctx, attrs["TEXCOORD_0"].asInt(), 2, uv);
        size_t count = pos.size() / 3;

        std::vector<uint32_t> idx;
        if (prim.has("indices")) {
            if (!readIndices(ctx, prim["indices"].asInt(), idx)) continue;
        } else {
            idx.resize(count);
            for (size_t i = 0; i < count; ++i) idx[i] = (uint32_t)i;
        }

        std::vector<Vertex> verts(count);
        for (size_t i = 0; i < count; ++i) {
            glm::vec3 lp(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
            verts[i].position = glm::vec3(world * glm::vec4(lp, 1.0f));
            if (hasNormals) verts[i].normal = glm::normalize(normalMatrix * glm::vec3(nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]));
            if (hasUVs) verts[i].uv = glm::vec2(uv[i * 2], 1.0f - uv[i * 2 + 1]); // glTF UV origin is top-left
        }
        if (flipWinding)
            for (size_t i = 0; i + 2 < idx.size(); i += 3) std::swap(idx[i + 1], idx[i + 2]);
        if (!hasNormals) Mesh::computeNormals(verts, idx);

        int material = prim.has("material") ? prim["material"].asInt() : defaultMaterial;
        Batch& batch = batches[material];
        uint32_t base = (uint32_t)batch.vertices.size();
        batch.vertices.insert(batch.vertices.end(), verts.begin(), verts.end());
        for (uint32_t i : idx) batch.indices.push_back(base + i);
    }
}

void visitNode(GltfContext& ctx, int nodeIndex, const glm::mat4& parent, std::unordered_map<int, Batch>& batches,
               int defaultMaterial, int depth) {
    if (depth > 64) return;
    const Json& node = ctx.cdoc()["nodes"][(size_t)nodeIndex];
    glm::mat4 world = parent * nodeLocalMatrix(node);
    if (node.has("mesh")) appendMesh(ctx, node["mesh"].asInt(), world, batches, defaultMaterial);
    const Json& children = node["children"];
    for (size_t i = 0; i < children.size(); ++i) visitNode(ctx, children[i].asInt(), world, batches, defaultMaterial, depth + 1);
}

} // namespace

Ref<Model> loadGLTF(const std::string& absolutePath) {
    GltfContext ctx;
    ctx.path = absolutePath;
    ctx.dir = FileSystem::directory(absolutePath);

    std::vector<uint8_t> file;
    if (!FileSystem::readBinary(absolutePath, file)) {
        LOG_ERROR("glTF not found: %s", absolutePath.c_str());
        return nullptr;
    }

    std::string jsonText;
    if (file.size() >= 12 && std::memcmp(file.data(), "glTF", 4) == 0) {
        // Binary container: 12-byte header followed by chunks.
        uint32_t version;
        std::memcpy(&version, file.data() + 4, 4);
        if (version != 2) {
            LOG_ERROR("GLB '%s' has unsupported version %u", absolutePath.c_str(), version);
            return nullptr;
        }
        size_t offset = 12;
        while (offset + 8 <= file.size()) {
            uint32_t chunkLength, chunkType;
            std::memcpy(&chunkLength, file.data() + offset, 4);
            std::memcpy(&chunkType, file.data() + offset + 4, 4);
            offset += 8;
            if (offset + chunkLength > file.size()) break;
            if (chunkType == 0x4E4F534A) jsonText.assign((const char*)file.data() + offset, chunkLength);       // "JSON"
            else if (chunkType == 0x004E4942) ctx.glbBin.assign(file.begin() + (std::ptrdiff_t)offset,
                                                                 file.begin() + (std::ptrdiff_t)(offset + chunkLength)); // "BIN"
            offset += (chunkLength + 3u) & ~3u;
        }
    } else {
        jsonText.assign((const char*)file.data(), file.size());
    }

    std::string error;
    if (!Json::parse(jsonText, ctx.doc, &error)) {
        LOG_ERROR("glTF '%s' JSON error: %s", absolutePath.c_str(), error.c_str());
        return nullptr;
    }
    if (!loadBuffers(ctx)) return nullptr;

    auto model = MakeRef<Model>();
    model->path = FileSystem::toAssetPath(absolutePath);

    // Materials
    const Json& mats = ctx.cdoc()["materials"];
    for (size_t i = 0; i < mats.size(); ++i) {
        const Json& m = mats[i];
        auto mat = MakeRef<Material>();
        mat->name = m["name"].asString("Material " + std::to_string(i));
        mat->metallic = 1.0f;  // glTF defaults
        mat->roughness = 1.0f;
        const Json& pbr = m["pbrMetallicRoughness"];
        if (pbr.isObject()) {
            const Json& bc = pbr["baseColorFactor"];
            if (bc.isArray()) mat->albedo = glm::vec4(bc[0].asFloat(1), bc[1].asFloat(1), bc[2].asFloat(1), bc[3].asFloat(1));
            mat->metallic = pbr["metallicFactor"].asFloat(1.0f);
            mat->roughness = pbr["roughnessFactor"].asFloat(1.0f);
            if (pbr.has("baseColorTexture")) mat->albedoTexture = loadImage(ctx, pbr["baseColorTexture"]["index"].asInt());
        }
        const Json& em = m["emissiveFactor"];
        if (em.isArray()) mat->emission = glm::vec3(em[0].asFloat(), em[1].asFloat(), em[2].asFloat());
        const Json& strength = m["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"];
        if (strength.isNumber()) mat->emission *= strength.asFloat(1.0f);
        if (m["extensions"].has("KHR_materials_unlit")) mat->shader = ShaderModel::Unlit;
        model->materials.push_back(mat);
    }
    int defaultMaterial = (int)model->materials.size();
    model->materials.push_back(MakeRef<Material>());

    // Geometry from the default scene's node hierarchy
    std::unordered_map<int, Batch> batches;
    const Json& scenes = ctx.cdoc()["scenes"];
    int sceneIndex = ctx.cdoc()["scene"].asInt(0);
    if (scenes.size() > 0 && scenes[(size_t)sceneIndex].isObject()) {
        const Json& roots = scenes[(size_t)sceneIndex]["nodes"];
        for (size_t i = 0; i < roots.size(); ++i) visitNode(ctx, roots[i].asInt(), glm::mat4(1.0f), batches, defaultMaterial, 0);
    } else {
        // No scene: draw every mesh once at the origin.
        for (size_t i = 0; i < ctx.cdoc()["meshes"].size(); ++i) appendMesh(ctx, (int)i, glm::mat4(1.0f), batches, defaultMaterial);
    }

    for (auto& kv : batches) {
        if (kv.second.indices.empty()) continue;
        SubMesh sm;
        int matIndex = kv.first >= 0 && kv.first < (int)model->materials.size() ? kv.first : defaultMaterial;
        sm.mesh = MakeRef<Mesh>(kv.second.vertices, kv.second.indices, model->materials[(size_t)matIndex]->name);
        sm.materialIndex = (uint32_t)matIndex;
        model->submeshes.push_back(sm);
    }
    if (model->submeshes.empty()) {
        LOG_ERROR("glTF '%s' contains no triangle meshes", absolutePath.c_str());
        return nullptr;
    }
    model->computeBounds();
    LOG_INFO("Loaded glTF '%s' (%zu sub-meshes, %zu materials)", model->path.c_str(), model->submeshes.size(),
             model->materials.size() - 1);
    return model;
}

} // namespace Forge
