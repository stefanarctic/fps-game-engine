#include "Forge/Renderer/Mesh.h"

#include <glad/gl.h>

namespace Forge {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name)
    : m_Name(name), m_Vertices(vertices), m_Indices(indices) {
    for (const Vertex& v : m_Vertices) m_Bounds.expand(v.position);

    glGenVertexArrays(1, &m_Vao);
    glGenBuffers(1, &m_Vbo);
    glGenBuffers(1, &m_Ibo);

    glBindVertexArray(m_Vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_Vertices.size() * sizeof(Vertex)), m_Vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m_Indices.size() * sizeof(uint32_t)), m_Indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_Ibo) glDeleteBuffers(1, &m_Ibo);
    if (m_Vbo) glDeleteBuffers(1, &m_Vbo);
    if (m_Vao) glDeleteVertexArrays(1, &m_Vao);
}

void Mesh::draw() const {
    glBindVertexArray(m_Vao);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_Indices.size(), GL_UNSIGNED_INT, nullptr);
}

bool Mesh::raycast(const Ray& ray, float& tOut) const {
    float tBox;
    if (!Math::rayAABB(ray, m_Bounds, tBox)) return false;
    bool hit = false;
    float best = FLT_MAX;
    for (size_t i = 0; i + 2 < m_Indices.size(); i += 3) {
        float t;
        if (Math::rayTriangle(ray, m_Vertices[m_Indices[i]].position, m_Vertices[m_Indices[i + 1]].position,
                              m_Vertices[m_Indices[i + 2]].position, t) && t < best) {
            best = t;
            hit = true;
        }
    }
    if (hit) tOut = best;
    return hit;
}

void Mesh::computeNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (Vertex& v : vertices) v.normal = glm::vec3(0.0f);
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        glm::vec3 n = glm::cross(b.position - a.position, c.position - a.position); // area weighted
        a.normal += n;
        b.normal += n;
        c.normal += n;
    }
    for (Vertex& v : vertices) {
        float len = glm::length(v.normal);
        v.normal = len > 1e-8f ? v.normal / len : glm::vec3(0, 1, 0);
    }
}

} // namespace Forge
