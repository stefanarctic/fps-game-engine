#include "Forge/Editor/EditorCamera.h"
#include "Forge/Core/Input.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace Forge::Editor {

bool EditorCamera::update(float dt, bool viewHovered) {
    ImGuiIO& io = ImGui::GetIO();
    glm::vec2 delta = Input::rawMouseDelta();

    // Start an interaction only when the mouse is over the scene view.
    if (m_Mode == Mode::None && viewHovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) m_Mode = Mode::Fly;
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) m_Mode = Mode::Pan;
        else if (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) m_Mode = Mode::Orbit;
        if (m_Mode == Mode::Fly) Input::setCursorLocked(true);
        if (m_Mode != Mode::None) m_Animating = false;
    }

    switch (m_Mode) {
    case Mode::Fly: {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            m_Mode = Mode::None;
            Input::setCursorLocked(false);
            break;
        }
        const float sensitivity = 0.15f;
        yaw -= delta.x * sensitivity;
        pitch = glm::clamp(pitch - delta.y * sensitivity, -89.0f, 89.0f);

        if (io.MouseWheel != 0.0f) flySpeed = glm::clamp(flySpeed * (io.MouseWheel > 0 ? 1.2f : 1.0f / 1.2f), 0.5f, 200.0f);

        glm::vec3 move(0.0f);
        if (ImGui::IsKeyDown(ImGuiKey_W)) move += forward();
        if (ImGui::IsKeyDown(ImGuiKey_S)) move -= forward();
        if (ImGui::IsKeyDown(ImGuiKey_D)) move += right();
        if (ImGui::IsKeyDown(ImGuiKey_A)) move -= right();
        if (ImGui::IsKeyDown(ImGuiKey_E)) move += glm::vec3(0, 1, 0);
        if (ImGui::IsKeyDown(ImGuiKey_Q)) move -= glm::vec3(0, 1, 0);
        if (glm::dot(move, move) > 0.0f) {
            float speed = flySpeed * (io.KeyShift ? 3.0f : 1.0f);
            position += glm::normalize(move) * speed * dt;
        }
        return true;
    }
    case Mode::Pan: {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            m_Mode = Mode::None;
            break;
        }
        float scale = std::max(pivotDistance, 1.0f) * 0.0018f;
        position += (-right() * delta.x + up() * delta.y) * scale;
        return true;
    }
    case Mode::Orbit: {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_Mode = Mode::None;
            break;
        }
        glm::vec3 p = pivot();
        yaw -= delta.x * 0.3f;
        pitch = glm::clamp(pitch - delta.y * 0.3f, -89.0f, 89.0f);
        position = p - forward() * pivotDistance;
        return true;
    }
    case Mode::None: break;
    }

    // Dolly with the scroll wheel.
    if (viewHovered && io.MouseWheel != 0.0f) {
        float amount = std::max(pivotDistance * 0.15f, 0.5f) * io.MouseWheel;
        position += forward() * amount;
        pivotDistance = std::max(pivotDistance - amount, 1.0f);
        m_Animating = false;
    }

    if (m_Animating) {
        position = glm::mix(position, m_TargetPosition, Math::damp(12.0f, dt));
        if (glm::distance(position, m_TargetPosition) < 0.01f) m_Animating = false;
    }
    return false;
}

void EditorCamera::focus(const glm::vec3& center, float radius) {
    pivotDistance = std::max(radius * 2.5f, 2.0f);
    m_TargetPosition = center - forward() * pivotDistance;
    m_Animating = true;
}

} // namespace Forge::Editor
