#include "Forge/Renderer/Texture.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"

#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Forge {

namespace {
struct GLFormat {
    GLint internal;
    GLenum format;
    GLenum type;
};

GLFormat glFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::RGBA8: return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::SRGBA8: return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case TextureFormat::R8: return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
    case TextureFormat::R16F: return {GL_R16F, GL_RED, GL_FLOAT};
    case TextureFormat::RG16F: return {GL_RG16F, GL_RG, GL_FLOAT};
    case TextureFormat::RGB16F: return {GL_RGB16F, GL_RGB, GL_FLOAT};
    case TextureFormat::RGBA16F: return {GL_RGBA16F, GL_RGBA, GL_FLOAT};
    case TextureFormat::Depth24: return {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT};
    case TextureFormat::Depth32F: return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
    }
    return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
}
} // namespace

Texture2D::Texture2D(const TextureSpec& spec, const void* pixels) : m_Spec(spec) {
    glGenTextures(1, &m_Id);
    allocate(pixels);
}

Texture2D::~Texture2D() {
    if (m_Id) glDeleteTextures(1, &m_Id);
}

void Texture2D::allocate(const void* pixels) {
    GLFormat f = glFormat(m_Spec.format);
    glBindTexture(GL_TEXTURE_2D, m_Id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, f.internal, m_Spec.width, m_Spec.height, 0, f.format, f.type, pixels);

    GLint minFilter = m_Spec.linear ? (m_Spec.mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) : (m_Spec.mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_Spec.linear ? GL_LINEAR : GL_NEAREST);
    GLint wrap = m_Spec.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    if (m_Spec.mipmaps && pixels) {
        glGenerateMipmap(GL_TEXTURE_2D);
        // 4x anisotropic filtering when available (core in GL 4.6, extension before).
        if (GLAD_GL_VERSION_3_3) {
            const GLenum kMaxAniso = 0x84FF, kAniso = 0x84FE;
            float maxAniso = 0.0f;
            glGetFloatv(kMaxAniso, &maxAniso);
            if (glGetError() == GL_NO_ERROR && maxAniso > 1.0f) glTexParameterf(GL_TEXTURE_2D, kAniso, maxAniso < 8.0f ? maxAniso : 8.0f);
            while (glGetError() != GL_NO_ERROR) {}
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::resize(int width, int height) {
    if (width == m_Spec.width && height == m_Spec.height) return;
    m_Spec.width = width < 1 ? 1 : width;
    m_Spec.height = height < 1 ? 1 : height;
    allocate(nullptr);
}

void Texture2D::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_Id);
}

Ref<Texture2D> Texture2D::fromRGBA(int width, int height, const uint8_t* rgba, bool srgb, bool mipmaps, bool repeat) {
    TextureSpec spec;
    spec.width = width;
    spec.height = height;
    spec.format = srgb ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;
    spec.mipmaps = mipmaps;
    spec.repeat = repeat;
    return MakeRef<Texture2D>(spec, rgba);
}

Ref<Texture2D> Texture2D::loadFromMemory(const uint8_t* data, size_t size, bool srgb, const std::string& name) {
    int w, h, comp;
    stbi_set_flip_vertically_on_load(1);
    uint8_t* pixels = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    if (!pixels) {
        LOG_ERROR("Failed to decode image '%s': %s", name.c_str(), stbi_failure_reason());
        return nullptr;
    }
    Ref<Texture2D> tex = fromRGBA(w, h, pixels, srgb, true, true);
    tex->path = name;
    stbi_image_free(pixels);
    return tex;
}

Ref<Texture2D> Texture2D::loadFromFile(const std::string& path, bool srgb) {
    std::vector<uint8_t> bytes;
    if (!FileSystem::readBinary(path, bytes)) {
        LOG_ERROR("Texture not found: %s", path.c_str());
        return nullptr;
    }
    return loadFromMemory(bytes.data(), bytes.size(), srgb, path);
}

} // namespace Forge
