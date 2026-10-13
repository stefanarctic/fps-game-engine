#pragma once
#include "Forge/Renderer/DebugDraw.h"
#include "Forge/Renderer/Framebuffer.h"
#include "Forge/Renderer/Material.h"
#include "Forge/Renderer/Shader.h"
#include "Forge/Scene/Scene.h"

namespace Forge {

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    float nearClip = 0.05f;
    float farClip = 500.0f;
    float fieldOfView = 60.0f;
    CameraClear clear = CameraClear::Skybox;
    glm::vec3 background{0.19f, 0.30f, 0.47f};

    // Builds camera data from an entity with Transform + Camera.
    static CameraData fromEntity(Scene& scene, Entity cameraEntity, float aspect);
};

enum class DebugView : int { Lit = 0, AmbientOcclusion = 1, Normals = 2, Albedo = 3, Lighting = 4 };

struct RenderOptions {
    DebugView debugView = DebugView::Lit;
    bool editor = false;                 // scene view: viewmodel drawn as world geometry
    bool grid = false;                   // infinite ground grid
    bool debugLines = true;              // global DebugDraw lines
    const LineBatch* gizmos = nullptr;   // extra lines (editor gizmos)
    std::vector<Entity> outline;         // entities drawn with a selection outline
    glm::vec4 outlineColor{1.0f, 0.55f, 0.1f, 1.0f};
};

struct RenderStats {
    int drawCalls = 0;
    int triangles = 0;
    int lights = 0;
    int shadowCasters = 0;
    int particles = 0;
};

// Renders a Scene from a camera into its own set of render targets. Create one
// SceneRenderer per view (editor Scene view, Game view, runtime window...).
//
// Frame graph:
//   1. Shadow map (directional light, 2048^2, 5x5 PCF)
//   2. Depth + view-space normal prepass
//   3. SSAO (hemisphere kernel, half resolution) + 4x4 blur
//   4. HDR forward pass: PBR Lit / Unlit materials, sky, grid, particles, lines
//   5. Viewmodel layer (depth cleared, drawn on top of the world)
//   6. Bloom (threshold + separable blur), ACES tone mapping, vignette
//   7. FXAA, then selection outline
class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    void render(Scene& scene, const CameraData& camera, int width, int height, const RenderOptions& options = RenderOptions());

    uint32_t outputTexture() const;
    int width() const { return m_Width; }
    int height() const { return m_Height; }
    const RenderStats& stats() const { return m_Stats; }

    static constexpr int kMaxLights = 16;
    static constexpr int kShadowMapSize = 2048;

private:
    struct DrawItem {
        const Mesh* mesh;
        Material* material;
        glm::mat4 model;
        glm::vec4 tint;
        AABB bounds;
        Entity entity;
        RenderLayer layer;
        bool castShadows;
    };
    struct LightItem {
        glm::vec3 position;
        glm::vec3 direction;
        glm::vec3 color;
        float range;
        float cosOuter;
        int type;
        float distance;
    };

    void ensureTargets(int width, int height);
    void collect(Scene& scene, const CameraData& camera, const RenderOptions& options);
    void shadowPass(Scene& scene, const CameraData& camera);
    void prepass(const CameraData& camera, const RenderOptions& options);
    void ssaoPass(Scene& scene, const CameraData& camera);
    void forwardPass(Scene& scene, const CameraData& camera, const RenderOptions& options);
    void drawItems(Scene& scene, const CameraData& camera, const RenderOptions& options, RenderLayer layer, bool useSSAO);
    void drawSky(Scene& scene, const CameraData& camera);
    void drawGrid(const CameraData& camera);
    void drawLines(const LineBatch& batch, const CameraData& camera, bool depthTest);
    void drawParticles(Scene& scene, const CameraData& camera);
    void postProcess(Scene& scene, const RenderOptions& options);
    void outlinePass(Scene& scene, const CameraData& camera, const RenderOptions& options);
    void drawFullscreen();

    int m_Width = 0, m_Height = 0;
    Scope<Framebuffer> m_ShadowFB, m_PrepassFB, m_SSAOFB, m_SSAOBlurFB, m_HdrFB;
    Scope<Framebuffer> m_BloomFB[2], m_LdrFB, m_OutputFB, m_MaskFB;

    uint32_t m_EmptyVao = 0;
    uint32_t m_LineVao = 0, m_LineVbo = 0;
    uint32_t m_ParticleVao = 0, m_ParticleVbo = 0;
    Scope<Texture2D> m_NoiseTexture;
    std::vector<glm::vec3> m_Kernel;

    std::vector<DrawItem> m_Items;
    std::vector<LightItem> m_Lights;
    bool m_HasSun = false;
    glm::vec3 m_SunDirection{0.0f, -1.0f, 0.0f};
    glm::vec3 m_SunColor{1.0f};
    bool m_SunShadows = false;
    glm::mat4 m_LightSpace{1.0f};
    bool m_SSAOValid = false;

    RenderStats m_Stats;
};

} // namespace Forge
