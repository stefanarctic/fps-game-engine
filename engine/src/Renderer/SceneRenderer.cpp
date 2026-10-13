#include "Forge/Renderer/SceneRenderer.h"
#include "Forge/Assets/AssetManager.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Settings.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <unordered_set>

namespace Forge {

namespace {
// Colors are authored in sRGB (like Unity's color pickers) and lit in linear space.
inline glm::vec3 toLinear(const glm::vec3& c) {
    return glm::vec3(std::pow(std::max(c.r, 0.0f), 2.2f), std::pow(std::max(c.g, 0.0f), 2.2f), std::pow(std::max(c.b, 0.0f), 2.2f));
}
inline glm::vec4 toLinear(const glm::vec4& c) { return glm::vec4(toLinear(glm::vec3(c)), c.a); }
// HDR colors: the hue is sRGB, the magnitude above 1 is a linear intensity multiplier.
inline glm::vec3 hdrToLinear(const glm::vec3& c) {
    glm::vec3 v = glm::max(c, glm::vec3(0.0f));
    float m = std::max(v.r, std::max(v.g, v.b));
    if (m <= 1.0f) return toLinear(v);
    return toLinear(v / m) * m;
}

Shader* useShader(const char* name) {
    Ref<Shader> s = AssetManager::getShader(name);
    if (!s || !s->valid()) return nullptr;
    s->bind();
    return s.get();
}

glm::vec3 cameraForward(const glm::mat4& view) {
    glm::mat4 inv = glm::inverse(view);
    return glm::normalize(-glm::vec3(inv[2]));
}
} // namespace

RenderOptions RenderOptions::fromSettings() {
    const GameSettings& s = Settings::get();
    RenderOptions o;
    o.shadows = s.shadows;
    o.ambientOcclusion = s.ambientOcclusion;
    o.bloom = s.bloom;
    o.antiAliasing = s.antiAliasing;
    return o;
}

CameraData CameraData::fromEntity(Scene& scene, Entity e, float aspect) {
    CameraData d;
    if (!scene.valid(e) || !scene.has<Camera>(e)) return d;
    const Transform& t = scene.get<Transform>(e);
    const Camera& cam = scene.get<Camera>(e);
    // Strip scale from the world matrix so a scaled parent does not skew the view.
    glm::mat4 rt(1.0f);
    rt[0] = glm::vec4(glm::normalize(glm::vec3(t.world[0])), 0.0f);
    rt[1] = glm::vec4(glm::normalize(glm::vec3(t.world[1])), 0.0f);
    rt[2] = glm::vec4(glm::normalize(glm::vec3(t.world[2])), 0.0f);
    rt[3] = t.world[3];
    d.view = glm::inverse(rt);
    d.projection = cam.projection(aspect);
    d.position = glm::vec3(t.world[3]);
    d.nearClip = cam.nearClip;
    d.farClip = cam.farClip;
    d.fieldOfView = cam.fieldOfView;
    d.clear = cam.clear;
    d.background = cam.background;
    return d;
}

SceneRenderer::SceneRenderer() {
    glGenVertexArrays(1, &m_EmptyVao);

    // Line batch buffers
    glGenVertexArrays(1, &m_LineVao);
    glGenBuffers(1, &m_LineVbo);
    glBindVertexArray(m_LineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, color));

    // Particle quads: position (3), color (4), uv (2)
    glGenVertexArrays(1, &m_ParticleVao);
    glGenBuffers(1, &m_ParticleVbo);
    glBindVertexArray(m_ParticleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_ParticleVbo);
    const GLsizei stride = 9 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glBindVertexArray(0);

    // SSAO hemisphere kernel: samples cluster near the origin.
    Random::seed(1337);
    for (int i = 0; i < 64; ++i) {
        glm::vec3 s(Random::range(-1.0f, 1.0f), Random::range(-1.0f, 1.0f), Random::value());
        s = glm::normalize(s) * Random::value();
        float scale = (float)i / 64.0f;
        scale = Math::lerp(0.1f, 1.0f, scale * scale);
        m_Kernel.push_back(s * scale);
    }
    // 4x4 random rotation vectors around the normal (tiled over the screen).
    std::vector<glm::vec3> noise;
    for (int i = 0; i < 16; ++i) noise.emplace_back(Random::range(-1.0f, 1.0f), Random::range(-1.0f, 1.0f), 0.0f);
    TextureSpec spec;
    spec.width = 4;
    spec.height = 4;
    spec.format = TextureFormat::RGB16F;
    spec.linear = false;
    spec.repeat = true;
    m_NoiseTexture = MakeScope<Texture2D>(spec, noise.data());
    Random::seed((uint32_t)std::time(nullptr));

    m_ShadowFB = MakeScope<Framebuffer>(kShadowMapSize, kShadowMapSize, std::vector<TextureFormat>{}, true, TextureFormat::Depth24, false);
}

SceneRenderer::~SceneRenderer() {
    glDeleteVertexArrays(1, &m_EmptyVao);
    glDeleteVertexArrays(1, &m_LineVao);
    glDeleteBuffers(1, &m_LineVbo);
    glDeleteVertexArrays(1, &m_ParticleVao);
    glDeleteBuffers(1, &m_ParticleVbo);
}

uint32_t SceneRenderer::outputTexture() const { return m_OutputFB ? m_OutputFB->colorTexture() : 0; }

void SceneRenderer::ensureTargets(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    int hw = std::max(width / 2, 1), hh = std::max(height / 2, 1);
    if (!m_HdrFB) {
        m_PrepassFB = MakeScope<Framebuffer>(width, height, std::vector<TextureFormat>{TextureFormat::RGB16F}, true, TextureFormat::Depth24, false);
        m_SSAOFB = MakeScope<Framebuffer>(hw, hh, std::vector<TextureFormat>{TextureFormat::R16F}, false);
        m_SSAOBlurFB = MakeScope<Framebuffer>(hw, hh, std::vector<TextureFormat>{TextureFormat::R16F}, false);
        m_HdrFB = MakeScope<Framebuffer>(width, height, std::vector<TextureFormat>{TextureFormat::RGBA16F}, true);
        m_BloomFB[0] = MakeScope<Framebuffer>(hw, hh, std::vector<TextureFormat>{TextureFormat::RGBA16F}, false);
        m_BloomFB[1] = MakeScope<Framebuffer>(hw, hh, std::vector<TextureFormat>{TextureFormat::RGBA16F}, false);
        m_LdrFB = MakeScope<Framebuffer>(width, height, std::vector<TextureFormat>{TextureFormat::RGBA8}, false);
        m_OutputFB = MakeScope<Framebuffer>(width, height, std::vector<TextureFormat>{TextureFormat::RGBA8}, false);
        m_MaskFB = MakeScope<Framebuffer>(width, height, std::vector<TextureFormat>{TextureFormat::R8}, false);
    } else {
        m_PrepassFB->resize(width, height);
        m_SSAOFB->resize(hw, hh);
        m_SSAOBlurFB->resize(hw, hh);
        m_HdrFB->resize(width, height);
        m_BloomFB[0]->resize(hw, hh);
        m_BloomFB[1]->resize(hw, hh);
        m_LdrFB->resize(width, height);
        m_OutputFB->resize(width, height);
        m_MaskFB->resize(width, height);
    }
    m_Width = width;
    m_Height = height;
}

void SceneRenderer::render(Scene& scene, const CameraData& camera, int width, int height, const RenderOptions& options) {
    ensureTargets(width, height);
    m_Stats = RenderStats();

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);

    collect(scene, camera, options);
    shadowPass(scene, camera);

    const Environment& env = scene.environment;
    bool needPrepass = (env.ssao && options.ambientOcclusion) || options.debugView == DebugView::AmbientOcclusion;
    m_SSAOValid = false;
    if (needPrepass) {
        prepass(camera, options);
        ssaoPass(scene, camera);
    }

    forwardPass(scene, camera, options);
    postProcess(scene, options);
    outlinePass(scene, camera, options);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFbo);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void SceneRenderer::collect(Scene& scene, const CameraData& camera, const RenderOptions& options) {
    m_Items.clear();
    m_Lights.clear();
    m_HasSun = false;

    scene.each<MeshRenderer, Transform>([&](Entity e, MeshRenderer& mr, Transform& t) {
        if (!mr.visible || !scene.isActive(e)) return;
        if (mr.mesh != mr.loadedMesh) {
            mr.loadedMesh = mr.mesh;
            mr.model = AssetManager::getModel(mr.mesh);
        }
        if (mr.material != mr.loadedMaterial) {
            mr.loadedMaterial = mr.material;
            mr.materialOverride = mr.material.empty() ? nullptr : AssetManager::getMaterial(mr.material);
        }
        if (!mr.model) return;
        for (const SubMesh& sm : mr.model->submeshes) {
            Material* mat = mr.materialOverride.get();
            if (!mat) mat = sm.materialIndex < mr.model->materials.size() ? mr.model->materials[sm.materialIndex].get() : nullptr;
            if (!mat) mat = AssetManager::defaultMaterial().get();
            AssetManager::resolveTextures(*mat);
            DrawItem item;
            item.mesh = sm.mesh.get();
            item.material = mat;
            item.model = t.world;
            item.tint = mr.tint;
            item.bounds = sm.mesh->bounds().transformed(t.world);
            item.entity = e;
            item.layer = mr.layer;
            item.castShadows = mr.castShadows;
            m_Items.push_back(item);
        }
    });

    scene.each<Light, Transform>([&](Entity e, Light& l, Transform& t) {
        if (!scene.isActive(e) || l.intensity <= 0.0f) return;
        glm::vec3 dir = t.forward();
        if (l.type == LightType::Directional) {
            if (!m_HasSun) {
                m_HasSun = true;
                m_SunDirection = dir;
                m_SunColor = toLinear(l.color) * l.intensity;
                m_SunShadows = l.castShadows && options.shadows;
            }
            return;
        }
        LightItem li;
        li.position = t.worldPosition();
        li.direction = dir;
        li.color = toLinear(l.color) * l.intensity;
        li.range = std::max(l.range, 0.01f);
        li.cosOuter = std::cos(glm::radians(glm::clamp(l.spotAngle, 1.0f, 179.0f) * 0.5f));
        li.type = (int)l.type;
        li.distance = glm::distance(camera.position, li.position) - li.range;
        m_Lights.push_back(li);
    });
    std::sort(m_Lights.begin(), m_Lights.end(), [](const LightItem& a, const LightItem& b) { return a.distance < b.distance; });
    if ((int)m_Lights.size() > kMaxLights) m_Lights.resize(kMaxLights);
    m_Stats.lights = (int)m_Lights.size() + (m_HasSun ? 1 : 0);
}

void SceneRenderer::shadowPass(Scene& scene, const CameraData& camera) {
    const Environment& env = scene.environment;
    if (!env.shadows || !m_HasSun || !m_SunShadows) {
        m_SunShadows = false;
        return;
    }
    Shader* shader = useShader("Depth");
    if (!shader) {
        m_SunShadows = false;
        return;
    }

    // Fit an orthographic box around the area in front of the camera. The box
    // center is snapped to shadow-map texels to avoid shimmering when moving.
    float radius = env.shadowDistance * 0.5f;
    glm::vec3 center = camera.position + cameraForward(camera.view) * radius * 0.8f;
    glm::vec3 L = glm::normalize(m_SunDirection);
    glm::vec3 up = std::fabs(L.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::mat4 lightRot = glm::lookAt(glm::vec3(0.0f), L, up);
    glm::vec3 c = glm::vec3(lightRot * glm::vec4(center, 1.0f));
    float texel = (2.0f * radius) / (float)kShadowMapSize;
    c.x = std::floor(c.x / texel) * texel;
    c.y = std::floor(c.y / texel) * texel;
    glm::mat4 proj = glm::ortho(c.x - radius, c.x + radius, c.y - radius, c.y + radius, -c.z - 200.0f, -c.z + 200.0f);
    m_LightSpace = proj * lightRot;

    Frustum lightFrustum;
    lightFrustum.fromMatrix(m_LightSpace);

    m_ShadowFB->bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    shader->setMat4("u_LightSpace", m_LightSpace);
    for (const DrawItem& item : m_Items) {
        if (!item.castShadows || item.layer != RenderLayer::World) continue;
        if (!lightFrustum.intersects(item.bounds)) continue;
        shader->setMat4("u_Model", item.model);
        item.mesh->draw();
        m_Stats.shadowCasters++;
        m_Stats.drawCalls++;
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void SceneRenderer::prepass(const CameraData& camera, const RenderOptions& options) {
    Shader* shader = useShader("Prepass");
    if (!shader) return;
    m_PrepassFB->bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Frustum frustum;
    frustum.fromMatrix(camera.projection * camera.view);
    shader->setMat4("u_ViewProj", camera.projection * camera.view);
    for (const DrawItem& item : m_Items) {
        if (item.layer != RenderLayer::World && !options.editor) continue;
        if (!frustum.intersects(item.bounds)) continue;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(camera.view * item.model)));
        shader->setMat4("u_Model", item.model);
        shader->setMat3("u_NormalMatrix", normalMatrix);
        item.mesh->draw();
        m_Stats.drawCalls++;
    }
}

void SceneRenderer::ssaoPass(Scene& scene, const CameraData& camera) {
    const Environment& env = scene.environment;
    Shader* shader = useShader("SSAO");
    if (!shader) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    m_SSAOFB->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_PrepassFB->depthTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_PrepassFB->colorTexture());
    m_NoiseTexture->bind(2);
    shader->setInt("u_Depth", 0);
    shader->setInt("u_Normal", 1);
    shader->setInt("u_Noise", 2);
    shader->setMat4("u_Projection", camera.projection);
    shader->setMat4("u_InvProjection", glm::inverse(camera.projection));
    shader->setVec3Array("u_Samples", m_Kernel.data(), (int)m_Kernel.size());
    shader->setInt("u_KernelSize", glm::clamp(env.ssaoSamples, 4, 64));
    shader->setFloat("u_Radius", env.ssaoRadius);
    shader->setFloat("u_Bias", env.ssaoBias);
    shader->setFloat("u_Power", env.ssaoPower);
    shader->setVec2("u_NoiseScale", glm::vec2(m_SSAOFB->width() / 4.0f, m_SSAOFB->height() / 4.0f));
    drawFullscreen();

    Shader* blur = useShader("SSAOBlur");
    if (!blur) return;
    m_SSAOBlurFB->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SSAOFB->colorTexture());
    blur->setInt("u_Input", 0);
    blur->setVec2("u_TexelSize", glm::vec2(1.0f / m_SSAOFB->width(), 1.0f / m_SSAOFB->height()));
    drawFullscreen();
    m_SSAOValid = true;
}

void SceneRenderer::forwardPass(Scene& scene, const CameraData& camera, const RenderOptions& options) {
    m_HdrFB->bind();
    glm::vec3 clear = camera.clear == CameraClear::SolidColor ? toLinear(camera.background) : glm::vec3(0.0f);
    if (options.debugView != DebugView::Lit && options.debugView != DebugView::Lighting) clear = glm::vec3(0.0f);
    glClearColor(clear.r, clear.g, clear.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);

    drawItems(scene, camera, options, RenderLayer::World, m_SSAOValid);

    bool lit = options.debugView == DebugView::Lit || options.debugView == DebugView::Lighting;
    if (camera.clear == CameraClear::Skybox && lit) drawSky(scene, camera);
    if (options.grid) drawGrid(camera);
    if (lit) drawParticles(scene, camera);
    if (options.debugLines) {
        const LineBatch& lines = DebugDraw::batch();
        if (!lines.empty()) drawLines(lines, camera, true);
    }
    if (options.gizmos && !options.gizmos->empty()) drawLines(*options.gizmos, camera, true);

    // First-person layer: drawn last on a cleared depth buffer so weapons never clip into walls.
    if (!options.editor) {
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        drawItems(scene, camera, options, RenderLayer::Viewmodel, false);
    }
}

void SceneRenderer::drawItems(Scene& scene, const CameraData& camera, const RenderOptions& options, RenderLayer layer, bool useSSAO) {
    const Environment& env = scene.environment;
    glm::mat4 viewProj = camera.projection * camera.view;
    Frustum frustum;
    frustum.fromMatrix(viewProj);

    auto wantsItem = [&](const DrawItem& item) {
        if (options.editor) return layer == RenderLayer::World; // editor: everything in one pass
        return item.layer == layer;
    };

    for (int pass = 0; pass < 2; ++pass) {
        ShaderModel model = pass == 0 ? ShaderModel::Lit : ShaderModel::Unlit;
        bool any = false;
        for (const DrawItem& item : m_Items)
            if (wantsItem(item) && item.material->shader == model) { any = true; break; }
        if (!any) continue;

        Shader* s = useShader(model == ShaderModel::Lit ? "Lit" : "Unlit");
        if (!s) continue;

        s->setMat4("u_ViewProj", viewProj);
        s->setVec3("u_CameraPos", camera.position);
        s->setInt("u_AlbedoMap", 0);
        s->setInt("u_DebugView", (int)options.debugView);
        s->setInt("u_FogEnabled", env.fog ? 1 : 0);
        s->setVec3("u_FogColor", toLinear(env.fogColor));
        s->setFloat("u_FogDensity", env.fogDensity);

        if (model == ShaderModel::Lit) {
            s->setVec3("u_SkyColor", toLinear(env.skyColor));
            s->setVec3("u_HorizonColor", toLinear(env.horizonColor));
            s->setVec3("u_GroundColor", toLinear(env.groundColor));
            s->setFloat("u_AmbientIntensity", env.ambientIntensity);

            s->setInt("u_HasSun", m_HasSun ? 1 : 0);
            s->setVec3("u_SunDirection", m_SunDirection);
            s->setVec3("u_SunColor", m_SunColor);

            glm::vec4 posRange[kMaxLights], colorType[kMaxLights], dirCone[kMaxLights];
            int count = (int)m_Lights.size();
            for (int i = 0; i < count; ++i) {
                const LightItem& l = m_Lights[(size_t)i];
                posRange[i] = glm::vec4(l.position, l.range);
                colorType[i] = glm::vec4(l.color, (float)l.type);
                dirCone[i] = glm::vec4(l.direction, l.cosOuter);
            }
            s->setInt("u_LightCount", count);
            if (count > 0) {
                s->setVec4Array("u_LightPosRange", posRange, count);
                s->setVec4Array("u_LightColorType", colorType, count);
                s->setVec4Array("u_LightDirCone", dirCone, count);
            }

            s->setInt("u_ShadowsEnabled", m_SunShadows ? 1 : 0);
            s->setMat4("u_LightSpace", m_LightSpace);
            s->setFloat("u_ShadowBias", env.shadowBias);
            s->setFloat("u_ShadowSoftness", env.shadowSoftness);
            s->setVec2("u_ShadowTexel", glm::vec2(1.0f / kShadowMapSize));
            s->setInt("u_ShadowMap", 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_ShadowFB->depthTexture());

            s->setInt("u_UseSSAO", useSSAO ? 1 : 0);
            s->setInt("u_SSAO", 2);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_SSAOValid ? m_SSAOBlurFB->colorTexture() : 0);
            s->setVec2("u_ScreenSize", glm::vec2((float)m_Width, (float)m_Height));
        }

        for (const DrawItem& item : m_Items) {
            if (!wantsItem(item) || item.material->shader != model) continue;
            if (!frustum.intersects(item.bounds)) continue;
            const Material& mat = *item.material;
            s->setMat4("u_Model", item.model);
            s->setMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(item.model))));
            glm::vec4 albedo = toLinear(mat.albedo) * toLinear(item.tint);
            s->setVec4("u_Albedo", albedo);
            s->setVec3("u_Emission", hdrToLinear(mat.emission));
            s->setVec2("u_Tiling", mat.tiling);
            s->setInt("u_WorldSpaceUV", mat.worldSpaceUV ? 1 : 0);
            if (model == ShaderModel::Lit) {
                s->setFloat("u_Metallic", mat.metallic);
                s->setFloat("u_Roughness", glm::clamp(mat.roughness, 0.03f, 1.0f));
            }
            Texture2D* tex = mat.albedoTexture ? mat.albedoTexture.get() : AssetManager::whiteTexture().get();
            tex->bind(0);
            item.mesh->draw();
            m_Stats.drawCalls++;
            m_Stats.triangles += (int)item.mesh->indexCount() / 3;
        }
    }
}

void SceneRenderer::drawSky(Scene& scene, const CameraData& camera) {
    Shader* s = useShader("Skybox");
    if (!s) return;
    const Environment& env = scene.environment;
    glm::mat4 viewRot = glm::mat4(glm::mat3(camera.view));
    s->setMat4("u_InvViewProj", glm::inverse(camera.projection * viewRot));
    s->setVec3("u_SkyColor", toLinear(env.skyColor) * env.skyIntensity);
    s->setVec3("u_HorizonColor", toLinear(env.horizonColor) * env.skyIntensity);
    s->setVec3("u_GroundColor", toLinear(env.groundColor) * env.skyIntensity);
    s->setInt("u_HasSun", m_HasSun ? 1 : 0);
    s->setVec3("u_SunDirection", m_SunDirection);
    s->setVec3("u_SunColor", m_SunColor);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    drawFullscreen();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void SceneRenderer::drawGrid(const CameraData& camera) {
    Shader* s = useShader("Grid");
    if (!s) return;
    glm::mat4 viewProj = camera.projection * camera.view;
    s->setMat4("u_ViewProj", viewProj);
    s->setMat4("u_InvViewProj", glm::inverse(viewProj));
    s->setVec3("u_CameraPos", camera.position);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawFullscreen();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void SceneRenderer::drawLines(const LineBatch& batch, const CameraData& camera, bool depthTest) {
    Shader* s = useShader("Lines");
    if (!s) return;
    const auto& verts = batch.vertices();
    std::vector<LineVertex> linear(verts.begin(), verts.end());
    for (LineVertex& v : linear) v.color = toLinear(v.color);
    s->setMat4("u_ViewProj", camera.projection * camera.view);
    glBindVertexArray(m_LineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_LineVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(linear.size() * sizeof(LineVertex)), linear.data(), GL_STREAM_DRAW);
    if (depthTest) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_LINES, 0, (GLsizei)linear.size());
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    m_Stats.drawCalls++;
}

void SceneRenderer::drawParticles(Scene& scene, const CameraData& camera) {
    std::vector<float> batches[2]; // [0] alpha blended (smoke, debris), [1] additive (fire, sparks)
    glm::vec3 right(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    glm::vec3 up(camera.view[0][1], camera.view[1][1], camera.view[2][1]);
    scene.each<ParticleEmitter>([&](Entity, ParticleEmitter& em) {
        std::vector<float>& data = batches[em.blend == ParticleBlend::Additive ? 1 : 0];
        for (const Particle& p : em.particles) {
            float t = glm::clamp(p.age / std::max(p.lifetime, 1e-4f), 0.0f, 1.0f);
            glm::vec4 c = glm::mix(em.startColor, em.endColor, t);
            c = glm::vec4(hdrToLinear(glm::vec3(c)), c.a);
            float size = Math::lerp(em.startSize, em.endSize, t) * 0.5f;
            if (size <= 0.0f || c.a <= 0.0f) continue;
            glm::vec3 r = right * size, u = up * size;
            const glm::vec3 corners[4] = {p.position - r - u, p.position + r - u, p.position + r + u, p.position - r + u};
            const glm::vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            const int order[6] = {0, 1, 2, 0, 2, 3};
            for (int k : order) {
                data.insert(data.end(), {corners[k].x, corners[k].y, corners[k].z, c.r, c.g, c.b, c.a, uvs[k].x, uvs[k].y});
            }
            m_Stats.particles++;
        }
    });
    if (batches[0].empty() && batches[1].empty()) return;
    Shader* s = useShader("Particles");
    if (!s) return;
    s->setMat4("u_ViewProj", camera.projection * camera.view);
    glBindVertexArray(m_ParticleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_ParticleVbo);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    for (int i = 0; i < 2; ++i) {
        const std::vector<float>& data = batches[i];
        if (data.empty()) continue;
        if (i == 0) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * sizeof(float)), data.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(data.size() / 9));
        m_Stats.drawCalls++;
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void SceneRenderer::postProcess(Scene& scene, const RenderOptions& options) {
    const Environment& env = scene.environment;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);

    // Debug views (AO / normals / albedo) bypass tone mapping so values read correctly.
    bool passthrough = options.debugView == DebugView::AmbientOcclusion || options.debugView == DebugView::Normals ||
                       options.debugView == DebugView::Albedo;
    bool bloom = env.bloom && options.bloom && env.bloomIntensity > 0.0f && !passthrough;
    if (bloom) {
        Shader* pre = useShader("BloomPrefilter");
        Shader* blur = AssetManager::getShader("Blur").get();
        if (pre && blur && blur->valid()) {
            m_BloomFB[0]->bind();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_HdrFB->colorTexture());
            pre->setInt("u_Input", 0);
            pre->setFloat("u_Threshold", env.bloomThreshold);
            pre->setVec2("u_TexelSize", glm::vec2(1.0f / m_HdrFB->width(), 1.0f / m_HdrFB->height()));
            drawFullscreen();

            blur->bind();
            blur->setInt("u_Input", 0);
            glm::vec2 texel(1.0f / m_BloomFB[0]->width(), 1.0f / m_BloomFB[0]->height());
            for (int i = 0; i < 3; ++i) {
                float spread = 1.0f + (float)i;
                m_BloomFB[1]->bind();
                glBindTexture(GL_TEXTURE_2D, m_BloomFB[0]->colorTexture());
                blur->setVec2("u_Direction", glm::vec2(texel.x * spread, 0.0f));
                drawFullscreen();
                m_BloomFB[0]->bind();
                glBindTexture(GL_TEXTURE_2D, m_BloomFB[1]->colorTexture());
                blur->setVec2("u_Direction", glm::vec2(0.0f, texel.y * spread));
                drawFullscreen();
            }
        } else {
            bloom = false;
        }
    }

    Shader* composite = useShader("Composite");
    if (composite) {
        m_LdrFB->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_HdrFB->colorTexture());
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_BloomFB[0]->colorTexture());
        composite->setInt("u_Scene", 0);
        composite->setInt("u_Bloom", 1);
        composite->setFloat("u_BloomIntensity", bloom ? env.bloomIntensity : 0.0f);
        composite->setFloat("u_Exposure", env.exposure);
        composite->setFloat("u_Vignette", passthrough ? 0.0f : env.vignette);
        composite->setInt("u_Passthrough", passthrough ? 1 : 0);
        drawFullscreen();
    }

    Shader* fxaa = useShader("FXAA");
    if (fxaa) {
        m_OutputFB->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_LdrFB->colorTexture());
        fxaa->setInt("u_Input", 0);
        fxaa->setInt("u_Enabled", env.fxaa && options.antiAliasing ? 1 : 0);
        fxaa->setVec2("u_TexelSize", glm::vec2(1.0f / m_Width, 1.0f / m_Height));
        drawFullscreen();
    }
    glDepthMask(GL_TRUE);
}

void SceneRenderer::outlinePass(Scene& scene, const CameraData& camera, const RenderOptions& options) {
    if (options.outline.empty()) return;
    std::unordered_set<Entity> selected;
    std::vector<Entity> stack(options.outline.begin(), options.outline.end());
    while (!stack.empty()) {
        Entity e = stack.back();
        stack.pop_back();
        if (!scene.valid(e) || !selected.insert(e).second) continue;
        for (Entity c : scene.get<Transform>(e).children) stack.push_back(c);
    }

    Shader* mask = useShader("Mask");
    Shader* outline = AssetManager::getShader("Outline").get();
    if (!mask || !outline || !outline->valid()) return;

    m_MaskFB->bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    mask->setMat4("u_ViewProj", camera.projection * camera.view);
    bool any = false;
    for (const DrawItem& item : m_Items) {
        if (!selected.count(item.entity)) continue;
        mask->setMat4("u_Model", item.model);
        item.mesh->draw();
        any = true;
    }
    if (!any) return;

    m_OutputFB->bind();
    outline->bind();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_MaskFB->colorTexture());
    outline->setInt("u_Mask", 0);
    outline->setVec2("u_TexelSize", glm::vec2(1.0f / m_Width, 1.0f / m_Height));
    outline->setVec4("u_Color", options.outlineColor);
    drawFullscreen();
    glDisable(GL_BLEND);
}

void SceneRenderer::drawFullscreen() {
    glBindVertexArray(m_EmptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace Forge
