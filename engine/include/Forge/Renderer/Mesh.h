#pragma once
#include "Forge/Core/Base.h"
#include "Forge/Math/Math.h"

namespace Forge {

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
};

// GPU triangle mesh (VAO + VBO + IBO). A CPU copy of the geometry is kept for
// editor picking and collider fitting.
class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name = "");
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

    const AABB& bounds() const { return m_Bounds; }
    uint32_t indexCount() const { return (uint32_t)m_Indices.size(); }
    uint32_t vertexCount() const { return (uint32_t)m_Vertices.size(); }
    const std::vector<Vertex>& vertices() const { return m_Vertices; }
    const std::vector<uint32_t>& indices() const { return m_Indices; }
    const std::string& name() const { return m_Name; }

    // Closest ray hit in mesh space (ray not necessarily normalized).
    bool raycast(const Ray& ray, float& t) const;

    // Recomputes smooth vertex normals from triangle geometry.
    static void computeNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);

private:
    std::string m_Name;
    uint32_t m_Vao = 0, m_Vbo = 0, m_Ibo = 0;
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;
    AABB m_Bounds;
};

} // namespace Forge
