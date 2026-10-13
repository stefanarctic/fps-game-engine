#include "Forge/Renderer/Framebuffer.h"
#include "Forge/Core/Log.h"

#include <glad/gl.h>

namespace Forge {

Framebuffer::Framebuffer(int width, int height, const std::vector<TextureFormat>& colors, bool depth,
                         TextureFormat depthFormat, bool linearFilter)
    : m_Width(width < 1 ? 1 : width), m_Height(height < 1 ? 1 : height) {
    for (TextureFormat f : colors) {
        TextureSpec spec;
        spec.width = m_Width;
        spec.height = m_Height;
        spec.format = f;
        spec.linear = linearFilter;
        m_Colors.push_back(MakeScope<Texture2D>(spec));
    }
    if (depth) {
        TextureSpec spec;
        spec.width = m_Width;
        spec.height = m_Height;
        spec.format = depthFormat;
        spec.linear = false;
        m_Depth = MakeScope<Texture2D>(spec);
    }
    glGenFramebuffers(1, &m_Fbo);
    attach();
}

Framebuffer::~Framebuffer() {
    if (m_Fbo) glDeleteFramebuffers(1, &m_Fbo);
}

void Framebuffer::attach() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_Fbo);
    std::vector<GLenum> drawBuffers;
    for (size_t i = 0; i < m_Colors.size(); ++i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, m_Colors[i]->id(), 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
    }
    if (m_Depth) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_Depth->id(), 0);
    if (drawBuffers.empty()) {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else {
        glDrawBuffers((GLsizei)drawBuffers.size(), drawBuffers.data());
    }
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) LOG_ERROR("Framebuffer incomplete (0x%x)", status);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::resize(int width, int height) {
    width = width < 1 ? 1 : width;
    height = height < 1 ? 1 : height;
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    for (auto& c : m_Colors) c->resize(width, height);
    if (m_Depth) m_Depth->resize(width, height);
    attach();
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_Fbo);
    glViewport(0, 0, m_Width, m_Height);
}

void Framebuffer::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

} // namespace Forge
