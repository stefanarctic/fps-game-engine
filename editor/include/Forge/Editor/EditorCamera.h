#pragma once
#include "Forge/Math/Math.h"

namespace Forge::Editor {

// Unity-style scene view camera:
//   RMB + mouse      look around          RMB + WASD/QE   fly (Shift = faster)
//   RMB + scroll     change fly speed     MMB drag        pan
//   Alt + LMB        orbit around pivot   Scroll          dolly towards pivot
//   F                frame selection (handled by the editor through focus())
class EditorCamera {
public:
    glm::vec3 position{-14.0f, 9.0f, 16.0f};
    float yaw = -40.0f;     // degrees, 0 = looking down -Z
    float pitch = -22.0f;   // degrees, positive looks up
    float fieldOfView = 60.0f;
    float nearClip = 0.05f;
    float farClip = 1000.0f;
    float flySpeed = 10.0f;
    float pivotDistance = 15.0f;

    // Returns true while the camera owns the mouse (flying / orbiting / panning).
    bool update(float dt, bool viewHovered);
    void focus(const glm::vec3& center, float radius);

    glm::vec3 forward() const { return Math::directionFromYawPitch(yaw, pitch); }
    glm::vec3 right() const { return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0))); }
    glm::vec3 up() const { return glm::normalize(glm::cross(right(), forward())); }
    glm::vec3 pivot() const { return position + forward() * pivotDistance; }
    glm::mat4 view() const { return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0)); }
    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(fieldOfView), aspect > 0.0f ? aspect : 1.0f, nearClip, farClip);
    }
    bool isFlying() const { return m_Mode == Mode::Fly; }

private:
    enum class Mode { None, Fly, Pan, Orbit };
    Mode m_Mode = Mode::None;
    // Smooth focus animation
    bool m_Animating = false;
    glm::vec3 m_TargetPosition{0.0f};
};

} // namespace Forge::Editor
