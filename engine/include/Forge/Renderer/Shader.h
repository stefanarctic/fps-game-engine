#pragma once
#include "Forge/Core/Base.h"
#include "Forge/Math/Math.h"

#include <unordered_map>

namespace Forge {

// GLSL program loaded from a single file containing several stages:
//
//   #version 330 core            <- shared prefix, copied into every stage
//   #type vertex
//   ...
//   #type fragment
//   ...
//
// `#include "File.glsl"` is resolved relative to the shader's directory.
class Shader {
public:
    ~Shader();

    static Ref<Shader> fromFile(const std::string& path); // absolute path
    static Ref<Shader> fromSource(const std::string& name, const std::string& vertex, const std::string& fragment);

    bool reload(); // recompiles from disk, keeps the old program on failure
    bool valid() const { return m_Program != 0; }
    const std::string& name() const { return m_Name; }
    const std::string& path() const { return m_Path; }
    int64_t fileTime() const { return m_FileTime; }

    void bind() const;
    uint32_t id() const { return m_Program; }

    void setInt(const char* name, int v);
    void setBool(const char* name, bool v) { setInt(name, v ? 1 : 0); }
    void setFloat(const char* name, float v);
    void setVec2(const char* name, const glm::vec2& v);
    void setVec3(const char* name, const glm::vec3& v);
    void setVec4(const char* name, const glm::vec4& v);
    void setMat3(const char* name, const glm::mat3& m);
    void setMat4(const char* name, const glm::mat4& m);
    void setVec3Array(const char* name, const glm::vec3* v, int count);
    void setVec4Array(const char* name, const glm::vec4* v, int count);

    int location(const char* name);

private:
    static uint32_t compile(const std::string& name, const std::string& vertex, const std::string& fragment);
    bool loadFile(std::string& vertex, std::string& fragment);

    std::string m_Name;
    std::string m_Path;
    int64_t m_FileTime = 0;
    uint32_t m_Program = 0;
    std::unordered_map<std::string, int> m_Locations;
};

} // namespace Forge
