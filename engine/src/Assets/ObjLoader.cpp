#include "Forge/Assets/AssetManager.h"
#include "Forge/Assets/ModelLoaders.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace Forge {

namespace {

struct ObjIndex {
    int v = 0, vt = 0, vn = 0;
    bool operator==(const ObjIndex& o) const { return v == o.v && vt == o.vt && vn == o.vn; }
};

struct ObjIndexHash {
    size_t operator()(const ObjIndex& i) const {
        return ((size_t)i.v * 73856093u) ^ ((size_t)i.vt * 19349663u) ^ ((size_t)i.vn * 83492791u);
    }
};

struct Group {
    std::string name;
    int material = 0;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<ObjIndex, uint32_t, ObjIndexHash> lookup;
    bool hasNormals = true;
};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Parses "v", "v/vt", "v//vn", "v/vt/vn" and converts to absolute 1-based indices.
bool parseFaceVertex(const char* token, int vCount, int vtCount, int vnCount, ObjIndex& out) {
    int values[3] = {0, 0, 0};
    const char* p = token;
    for (int k = 0; k < 3; ++k) {
        if (*p && *p != '/') {
            char* end;
            values[k] = (int)std::strtol(p, &end, 10);
            p = end;
        }
        if (*p == '/') ++p;
        else break;
    }
    auto fix = [](int idx, int count) { return idx < 0 ? count + idx + 1 : idx; };
    out.v = fix(values[0], vCount);
    out.vt = fix(values[1], vtCount);
    out.vn = fix(values[2], vnCount);
    return out.v > 0 && out.v <= vCount;
}

void loadMTL(const std::string& path, std::vector<Ref<Material>>& materials, std::unordered_map<std::string, int>& byName) {
    std::string text;
    if (!FileSystem::readText(path, text)) {
        LOG_WARN("OBJ material library not found: %s", path.c_str());
        return;
    }
    std::string dir = FileSystem::directory(path);
    std::istringstream in(text);
    std::string line;
    Ref<Material> current;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string key;
        ls >> key;
        if (key == "newmtl") {
            current = MakeRef<Material>();
            std::string name;
            std::getline(ls, name);
            current->name = trim(name);
            byName[current->name] = (int)materials.size();
            materials.push_back(current);
        } else if (!current) {
            continue;
        } else if (key == "Kd") {
            ls >> current->albedo.r >> current->albedo.g >> current->albedo.b;
        } else if (key == "Ke") {
            ls >> current->emission.r >> current->emission.g >> current->emission.b;
        } else if (key == "Ns") {
            float ns = 0.0f;
            ls >> ns; // Blinn-Phong exponent -> roughness approximation
            current->roughness = glm::clamp(std::sqrt(2.0f / (ns + 2.0f)), 0.04f, 1.0f);
        } else if (key == "Pr") {
            ls >> current->roughness;
        } else if (key == "Pm") {
            ls >> current->metallic;
        } else if (key == "d") {
            ls >> current->albedo.a;
        } else if (key == "Tr") {
            float tr = 0.0f;
            ls >> tr;
            current->albedo.a = 1.0f - tr;
        } else if (key == "map_Kd") {
            std::string file;
            std::getline(ls, file);
            file = trim(file);
            // Options such as "-s 1 1 1" precede the file name; keep the last token.
            size_t sp = file.find_last_of(' ');
            if (sp != std::string::npos && file[0] == '-') file = file.substr(sp + 1);
            std::string abs = FileSystem::join(dir, file);
            current->albedoMap = FileSystem::toAssetPath(abs);
        }
    }
}
} // namespace

Ref<Model> loadOBJ(const std::string& absolutePath) {
    std::string text;
    if (!FileSystem::readText(absolutePath, text)) {
        LOG_ERROR("OBJ not found: %s", absolutePath.c_str());
        return nullptr;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<Ref<Material>> materials;
    std::unordered_map<std::string, int> materialByName;
    std::vector<Group> groups;
    std::unordered_map<int, size_t> groupByMaterial;

    auto groupFor = [&](int material) -> Group& {
        auto it = groupByMaterial.find(material);
        if (it != groupByMaterial.end()) return groups[it->second];
        groupByMaterial[material] = groups.size();
        groups.emplace_back();
        groups.back().material = material;
        return groups.back();
    };

    int currentMaterial = -1;
    std::string dir = FileSystem::directory(absolutePath);
    std::istringstream in(text);
    std::string line;
    std::vector<ObjIndex> face;
    size_t lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (line.empty() || line[0] == '#') continue;
        const char* s = line.c_str();
        while (*s == ' ' || *s == '\t') ++s;

        if (s[0] == 'v' && (s[1] == ' ' || s[1] == '\t')) {
            glm::vec3 p;
            char* end;
            p.x = std::strtof(s + 2, &end);
            p.y = std::strtof(end, &end);
            p.z = std::strtof(end, &end);
            positions.push_back(p);
        } else if (s[0] == 'v' && s[1] == 't') {
            glm::vec2 t;
            char* end;
            t.x = std::strtof(s + 3, &end);
            t.y = std::strtof(end, &end);
            uvs.push_back(t);
        } else if (s[0] == 'v' && s[1] == 'n') {
            glm::vec3 n;
            char* end;
            n.x = std::strtof(s + 3, &end);
            n.y = std::strtof(end, &end);
            n.z = std::strtof(end, &end);
            normals.push_back(n);
        } else if (s[0] == 'f' && (s[1] == ' ' || s[1] == '\t')) {
            face.clear();
            std::istringstream fs(s + 2);
            std::string token;
            while (fs >> token) {
                ObjIndex idx;
                if (!parseFaceVertex(token.c_str(), (int)positions.size(), (int)uvs.size(), (int)normals.size(), idx)) {
                    LOG_WARN("%s:%zu invalid face index '%s'", absolutePath.c_str(), lineNo, token.c_str());
                    face.clear();
                    break;
                }
                face.push_back(idx);
            }
            if (face.size() < 3) continue;

            if (currentMaterial < 0) {
                // Faces before any usemtl get a default material.
                if (!materialByName.count("__default")) {
                    auto m = MakeRef<Material>();
                    m->name = "Default";
                    materialByName["__default"] = (int)materials.size();
                    materials.push_back(m);
                }
                currentMaterial = materialByName["__default"];
            }
            Group& g = groupFor(currentMaterial);
            uint32_t ids[64];
            size_t count = face.size() < 64 ? face.size() : 64;
            for (size_t k = 0; k < count; ++k) {
                const ObjIndex& fi = face[k];
                auto it = g.lookup.find(fi);
                if (it != g.lookup.end()) {
                    ids[k] = it->second;
                    continue;
                }
                Vertex v;
                v.position = positions[(size_t)fi.v - 1];
                if (fi.vt > 0 && fi.vt <= (int)uvs.size()) v.uv = uvs[(size_t)fi.vt - 1];
                if (fi.vn > 0 && fi.vn <= (int)normals.size()) v.normal = normals[(size_t)fi.vn - 1];
                else g.hasNormals = false;
                uint32_t id = (uint32_t)g.vertices.size();
                g.vertices.push_back(v);
                g.lookup[fi] = id;
                ids[k] = id;
            }
            for (size_t k = 1; k + 1 < count; ++k) {
                g.indices.push_back(ids[0]);
                g.indices.push_back(ids[k]);
                g.indices.push_back(ids[k + 1]);
            }
        } else if (std::strncmp(s, "mtllib", 6) == 0) {
            std::string file = trim(std::string(s + 6));
            loadMTL(FileSystem::join(dir, file), materials, materialByName);
        } else if (std::strncmp(s, "usemtl", 6) == 0) {
            std::string name = trim(std::string(s + 6));
            auto it = materialByName.find(name);
            if (it != materialByName.end()) {
                currentMaterial = it->second;
            } else {
                auto m = MakeRef<Material>();
                m->name = name;
                currentMaterial = (int)materials.size();
                materialByName[name] = currentMaterial;
                materials.push_back(m);
            }
        }
        // o / g / s lines are accepted but grouping is done by material.
    }

    auto model = MakeRef<Model>();
    model->path = FileSystem::toAssetPath(absolutePath);
    model->materials = materials;
    for (Group& g : groups) {
        if (g.indices.empty()) continue;
        if (!g.hasNormals) Mesh::computeNormals(g.vertices, g.indices);
        SubMesh sm;
        sm.mesh = MakeRef<Mesh>(g.vertices, g.indices, materials[(size_t)g.material]->name);
        sm.materialIndex = (uint32_t)g.material;
        model->submeshes.push_back(sm);
    }
    if (model->submeshes.empty()) {
        LOG_ERROR("OBJ '%s' contains no faces", absolutePath.c_str());
        return nullptr;
    }
    if (model->materials.empty()) model->materials.push_back(MakeRef<Material>());
    model->computeBounds();
    LOG_INFO("Loaded OBJ '%s' (%zu vertices, %zu sub-meshes, %zu materials)", model->path.c_str(), positions.size(),
             model->submeshes.size(), model->materials.size());
    return model;
}

} // namespace Forge
