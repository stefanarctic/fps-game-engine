#include "Forge/Renderer/Shader.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"

#include <glad/gl.h>
#include <sstream>

namespace Forge {

namespace {
bool expandIncludes(const std::string& source, const std::string& dir, std::string& out, int depth) {
    if (depth > 8) {
        LOG_ERROR("Shader include depth exceeded");
        return false;
    }
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        size_t pos = line.find("#include");
        if (pos != std::string::npos && line.find_first_not_of(" \t") == pos) {
            size_t q1 = line.find('"', pos);
            size_t q2 = q1 == std::string::npos ? q1 : line.find('"', q1 + 1);
            if (q1 == std::string::npos || q2 == std::string::npos) {
                LOG_ERROR("Malformed #include: %s", line.c_str());
                return false;
            }
            std::string file = FileSystem::join(dir, line.substr(q1 + 1, q2 - q1 - 1));
            std::string text;
            if (!FileSystem::readText(file, text)) {
                LOG_ERROR("Shader include not found: %s", file.c_str());
                return false;
            }
            if (!expandIncludes(text, FileSystem::directory(file), out, depth + 1)) return false;
            continue;
        }
        out += line;
        out += '\n';
    }
    return true;
}

uint32_t compileStage(GLenum type, const std::string& src, const std::string& name) {
    uint32_t s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOG_ERROR("%s shader compile error in '%s':\n%s", type == GL_VERTEX_SHADER ? "Vertex" : "Fragment", name.c_str(), log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
} // namespace

Shader::~Shader() {
    if (m_Program) glDeleteProgram(m_Program);
}

uint32_t Shader::compile(const std::string& name, const std::string& vertex, const std::string& fragment) {
    uint32_t vs = compileStage(GL_VERTEX_SHADER, vertex, name);
    if (!vs) return 0;
    uint32_t fs = compileStage(GL_FRAGMENT_SHADER, fragment, name);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }
    uint32_t program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        LOG_ERROR("Shader link error in '%s':\n%s", name.c_str(), log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool Shader::loadFile(std::string& vertex, std::string& fragment) {
    std::string raw;
    if (!FileSystem::readText(m_Path, raw)) {
        LOG_ERROR("Cannot read shader '%s'", m_Path.c_str());
        return false;
    }
    std::string text;
    if (!expandIncludes(raw, FileSystem::directory(m_Path), text, 0)) return false;

    // Split into shared prefix + stages.
    std::string prefix, *current = &prefix;
    std::string vs, fs;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        size_t pos = line.find("#type");
        if (pos != std::string::npos && line.find_first_not_of(" \t") == pos) {
            if (line.find("vertex") != std::string::npos) current = &vs;
            else if (line.find("fragment") != std::string::npos || line.find("pixel") != std::string::npos) current = &fs;
            else {
                LOG_ERROR("Unknown shader stage in '%s': %s", m_Path.c_str(), line.c_str());
                return false;
            }
            *current += "#line " + std::to_string(lineNo + 1) + "\n";
            continue;
        }
        *current += line;
        *current += '\n';
    }
    if (vs.empty() || fs.empty()) {
        LOG_ERROR("Shader '%s' needs both '#type vertex' and '#type fragment' sections", m_Path.c_str());
        return false;
    }
    // The #version directive must stay first, so the #line markers go after the prefix.
    vertex = prefix + vs;
    fragment = prefix + fs;
    return true;
}

Ref<Shader> Shader::fromFile(const std::string& path) {
    auto shader = Ref<Shader>(new Shader());
    shader->m_Path = path;
    shader->m_Name = FileSystem::stem(path);
    if (!shader->reload()) return nullptr;
    return shader;
}

Ref<Shader> Shader::fromSource(const std::string& name, const std::string& vertex, const std::string& fragment) {
    auto shader = Ref<Shader>(new Shader());
    shader->m_Name = name;
    shader->m_Program = compile(name, vertex, fragment);
    if (!shader->m_Program) return nullptr;
    return shader;
}

bool Shader::reload() {
    if (m_Path.empty()) return valid();
    std::string vs, fs;
    m_FileTime = FileSystem::lastWriteTime(m_Path);
    if (!loadFile(vs, fs)) return false;
    uint32_t program = compile(m_Name, vs, fs);
    if (!program) return false;
    if (m_Program) glDeleteProgram(m_Program);
    m_Program = program;
    m_Locations.clear();
    return true;
}

void Shader::bind() const { glUseProgram(m_Program); }

int Shader::location(const char* name) {
    auto it = m_Locations.find(name);
    if (it != m_Locations.end()) return it->second;
    int loc = glGetUniformLocation(m_Program, name);
    m_Locations.emplace(name, loc);
    return loc;
}

void Shader::setInt(const char* name, int v) { glUniform1i(location(name), v); }
void Shader::setFloat(const char* name, float v) { glUniform1f(location(name), v); }
void Shader::setVec2(const char* name, const glm::vec2& v) { glUniform2fv(location(name), 1, &v.x); }
void Shader::setVec3(const char* name, const glm::vec3& v) { glUniform3fv(location(name), 1, &v.x); }
void Shader::setVec4(const char* name, const glm::vec4& v) { glUniform4fv(location(name), 1, &v.x); }
void Shader::setMat3(const char* name, const glm::mat3& m) { glUniformMatrix3fv(location(name), 1, GL_FALSE, &m[0][0]); }
void Shader::setMat4(const char* name, const glm::mat4& m) { glUniformMatrix4fv(location(name), 1, GL_FALSE, &m[0][0]); }
void Shader::setVec3Array(const char* name, const glm::vec3* v, int count) { glUniform3fv(location(name), count, &v[0].x); }
void Shader::setVec4Array(const char* name, const glm::vec4* v, int count) { glUniform4fv(location(name), count, &v[0].x); }

} // namespace Forge
