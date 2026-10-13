#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

enum class TextureFormat { RGBA8, SRGBA8, R8, R16F, RG16F, RGB16F, RGBA16F, Depth24, Depth32F };

struct TextureSpec {
    int width = 1;
    int height = 1;
    TextureFormat format = TextureFormat::RGBA8;
    bool mipmaps = false;
    bool linear = true;      // linear vs nearest filtering
    bool repeat = false;     // repeat vs clamp-to-edge
};

class Texture2D {
public:
    Texture2D(const TextureSpec& spec, const void* pixels = nullptr);
    ~Texture2D();
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    // Loads PNG/JPG/TGA/BMP via stb_image. Color textures should use srgb = true.
    static Ref<Texture2D> loadFromFile(const std::string& path, bool srgb = true);
    static Ref<Texture2D> loadFromMemory(const uint8_t* data, size_t size, bool srgb = true, const std::string& name = "");
    static Ref<Texture2D> fromRGBA(int width, int height, const uint8_t* rgba, bool srgb, bool mipmaps = true, bool repeat = true);

    void bind(int unit) const;
    void resize(int width, int height); // reallocates storage (render targets)
    uint32_t id() const { return m_Id; }
    int width() const { return m_Spec.width; }
    int height() const { return m_Spec.height; }
    const TextureSpec& spec() const { return m_Spec; }

    std::string path;

private:
    void allocate(const void* pixels);
    TextureSpec m_Spec;
    uint32_t m_Id = 0;
};

} // namespace Forge
