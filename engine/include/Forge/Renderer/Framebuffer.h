#pragma once
#include "Forge/Renderer/Texture.h"

namespace Forge {

// Render target with N color textures and an optional depth texture.
class Framebuffer {
public:
    Framebuffer(int width, int height, const std::vector<TextureFormat>& colors, bool depth,
                TextureFormat depthFormat = TextureFormat::Depth24, bool linearFilter = true);
    ~Framebuffer();
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void resize(int width, int height);
    void bind() const;   // binds and sets the viewport
    static void unbind();

    uint32_t id() const { return m_Fbo; }
    uint32_t colorTexture(size_t index = 0) const { return index < m_Colors.size() ? m_Colors[index]->id() : 0; }
    uint32_t depthTexture() const { return m_Depth ? m_Depth->id() : 0; }
    Texture2D* color(size_t index = 0) const { return index < m_Colors.size() ? m_Colors[index].get() : nullptr; }
    int width() const { return m_Width; }
    int height() const { return m_Height; }

private:
    void attach();
    uint32_t m_Fbo = 0;
    int m_Width = 0, m_Height = 0;
    std::vector<Scope<Texture2D>> m_Colors;
    Scope<Texture2D> m_Depth;
};

} // namespace Forge
