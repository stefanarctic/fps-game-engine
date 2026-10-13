#pragma once
#include "Forge/Core/Base.h"
#include "Forge/Math/Math.h"

namespace Forge {

struct LineVertex {
    glm::vec3 position;
    glm::vec4 color;
};

// A list of colored line segments rendered in one draw call.
class LineBatch {
public:
    void line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    void box(const AABB& box, const glm::vec4& color);
    void box(const glm::mat4& world, const AABB& localBox, const glm::vec4& color);
    void circle(const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec4& color, int segments = 32);
    void sphere(const glm::vec3& center, float radius, const glm::vec4& color);
    void arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    void frustum(const glm::mat4& invViewProj, const glm::vec4& color);
    void cone(const glm::vec3& apex, const glm::vec3& dir, float length, float angleDeg, const glm::vec4& color);

    void clear() { m_Vertices.clear(); }
    bool empty() const { return m_Vertices.empty(); }
    const std::vector<LineVertex>& vertices() const { return m_Vertices; }

private:
    std::vector<LineVertex> m_Vertices;
};

// Global debug lines visible in both the Scene and Game views.
// Lines with a duration persist for that many seconds (e.g. bullet tracers).
class DebugDraw {
public:
    static void line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color, float duration = 0.0f);
    static void box(const AABB& box, const glm::vec4& color, float duration = 0.0f);
    static void sphere(const glm::vec3& center, float radius, const glm::vec4& color, float duration = 0.0f);

    // Engine internal: collects the lines to render this frame and ages timed lines.
    static const LineBatch& batch();
    static void endFrame(float dt);
    static void clearAll();
};

} // namespace Forge
