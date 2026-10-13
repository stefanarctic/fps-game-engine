#include "Forge/Renderer/DebugDraw.h"

#include <cmath>

namespace Forge {

void LineBatch::line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
    m_Vertices.push_back({a, color});
    m_Vertices.push_back({b, color});
}

void LineBatch::box(const AABB& b, const glm::vec4& color) { box(glm::mat4(1.0f), b, color); }

void LineBatch::box(const glm::mat4& world, const AABB& b, const glm::vec4& color) {
    glm::vec3 c[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec3 p((i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z);
        c[i] = glm::vec3(world * glm::vec4(p, 1.0f));
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (auto& e : edges) line(c[e[0]], c[e[1]], color);
}

void LineBatch::circle(const glm::vec3& center, const glm::vec3& normal, float radius, const glm::vec4& color, int segments) {
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 t = std::fabs(n.y) < 0.99f ? glm::normalize(glm::cross(n, glm::vec3(0, 1, 0))) : glm::vec3(1, 0, 0);
    glm::vec3 b = glm::cross(n, t);
    glm::vec3 prev = center + t * radius;
    for (int i = 1; i <= segments; ++i) {
        float a = 2.0f * Math::PI * (float)i / (float)segments;
        glm::vec3 p = center + (t * std::cos(a) + b * std::sin(a)) * radius;
        line(prev, p, color);
        prev = p;
    }
}

void LineBatch::sphere(const glm::vec3& center, float radius, const glm::vec4& color) {
    circle(center, {1, 0, 0}, radius, color);
    circle(center, {0, 1, 0}, radius, color);
    circle(center, {0, 0, 1}, radius, color);
}

void LineBatch::arrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color) {
    line(from, to, color);
    glm::vec3 d = to - from;
    float len = glm::length(d);
    if (len < 1e-5f) return;
    d /= len;
    glm::vec3 side = std::fabs(d.y) < 0.99f ? glm::normalize(glm::cross(d, glm::vec3(0, 1, 0))) : glm::vec3(1, 0, 0);
    glm::vec3 up = glm::cross(side, d);
    float head = len * 0.2f;
    line(to, to - d * head + side * head * 0.4f, color);
    line(to, to - d * head - side * head * 0.4f, color);
    line(to, to - d * head + up * head * 0.4f, color);
    line(to, to - d * head - up * head * 0.4f, color);
}

void LineBatch::frustum(const glm::mat4& invViewProj, const glm::vec4& color) {
    glm::vec3 c[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 p((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f, 1.0f);
        glm::vec4 w = invViewProj * p;
        c[i] = glm::vec3(w) / w.w;
    }
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (auto& e : edges) line(c[e[0]], c[e[1]], color);
}

void LineBatch::cone(const glm::vec3& apex, const glm::vec3& dir, float length, float angleDeg, const glm::vec4& color) {
    glm::vec3 d = glm::normalize(dir);
    float radius = std::tan(glm::radians(angleDeg * 0.5f)) * length;
    glm::vec3 base = apex + d * length;
    circle(base, d, radius, color, 24);
    glm::vec3 t = std::fabs(d.y) < 0.99f ? glm::normalize(glm::cross(d, glm::vec3(0, 1, 0))) : glm::vec3(1, 0, 0);
    glm::vec3 b = glm::cross(d, t);
    line(apex, base + t * radius, color);
    line(apex, base - t * radius, color);
    line(apex, base + b * radius, color);
    line(apex, base - b * radius, color);
}

// ---------------------------------------------------------------------------
namespace {
struct TimedLine {
    glm::vec3 a, b;
    glm::vec4 color;
    float remaining;
};
struct DebugState {
    LineBatch batch;
    std::vector<TimedLine> timed;
};
DebugState& D() {
    static DebugState s;
    return s;
}
} // namespace

void DebugDraw::line(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color, float duration) {
    if (duration > 0.0f) D().timed.push_back({a, b, color, duration});
    else D().batch.line(a, b, color);
}

void DebugDraw::box(const AABB& box, const glm::vec4& color, float duration) {
    LineBatch tmp;
    tmp.box(box, color);
    const auto& v = tmp.vertices();
    for (size_t i = 0; i + 1 < v.size(); i += 2) line(v[i].position, v[i + 1].position, color, duration);
}

void DebugDraw::sphere(const glm::vec3& center, float radius, const glm::vec4& color, float duration) {
    LineBatch tmp;
    tmp.sphere(center, radius, color);
    const auto& v = tmp.vertices();
    for (size_t i = 0; i + 1 < v.size(); i += 2) line(v[i].position, v[i + 1].position, color, duration);
}

const LineBatch& DebugDraw::batch() {
    // Timed lines are appended lazily so they render every frame they are alive.
    static LineBatch combined;
    combined = D().batch;
    for (const TimedLine& t : D().timed) {
        glm::vec4 c = t.color;
        c.a *= glm::clamp(t.remaining * 10.0f, 0.0f, 1.0f); // fade out
        combined.line(t.a, t.b, c);
    }
    return combined;
}

void DebugDraw::endFrame(float dt) {
    D().batch.clear();
    auto& timed = D().timed;
    for (size_t i = 0; i < timed.size();) {
        timed[i].remaining -= dt;
        if (timed[i].remaining <= 0.0f) {
            timed[i] = timed.back();
            timed.pop_back();
        } else {
            ++i;
        }
    }
}

void DebugDraw::clearAll() {
    D().batch.clear();
    D().timed.clear();
}

} // namespace Forge
