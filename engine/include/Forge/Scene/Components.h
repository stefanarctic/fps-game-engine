#pragma once
// Built-in engine components. Components are plain data; behaviour lives in Systems.
#include "Forge/Core/Base.h"
#include "Forge/ECS/Registry.h"
#include "Forge/Math/Math.h"

namespace Forge {

class Model;
class Material;

// Every entity has one. Name/tag/active state + a persistent UUID used by
// serialization (entity handles are not stable across save/load).
struct EntityInfo {
    std::string name = "GameObject";
    std::string tag = "Untagged";
    bool active = true;
    uint64_t uuid = 0;
    bool activeInHierarchy = true; // computed: active && parent active
};

// Every entity has one. Rotation is stored as Euler angles in degrees and
// applied Y * X * Z (yaw, pitch, roll) like Unity. Forward is -Z (OpenGL).
struct Transform {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};

    // Hierarchy (managed by Scene::setParent, not edited directly)
    Entity parent = NullEntity;
    std::vector<Entity> children;

    // Computed every frame by Scene::updateTransforms
    glm::mat4 world{1.0f};

    glm::mat4 localMatrix() const { return Math::compose(position, rotation, scale); }
    glm::vec3 worldPosition() const { return glm::vec3(world[3]); }
    glm::vec3 forward() const { return glm::normalize(-glm::vec3(world[2])); }
    glm::vec3 right() const { return glm::normalize(glm::vec3(world[0])); }
    glm::vec3 up() const { return glm::normalize(glm::vec3(world[1])); }
};

enum class CameraClear : int { Skybox = 0, SolidColor = 1 };

struct Camera {
    float fieldOfView = 60.0f; // vertical, degrees
    float nearClip = 0.05f;
    float farClip = 500.0f;
    bool primary = true;
    CameraClear clear = CameraClear::Skybox;
    glm::vec3 background{0.19f, 0.30f, 0.47f};

    glm::mat4 projection(float aspect) const {
        return glm::perspective(glm::radians(fieldOfView), aspect > 0.0f ? aspect : 1.0f, nearClip, farClip);
    }
};

enum class RenderLayer : int { World = 0, Viewmodel = 1 };

struct MeshRenderer {
    std::string mesh = "builtin:Cube";   // asset path or builtin:Cube/Sphere/Plane/Quad/Cylinder/Capsule
    std::string material;                // optional .mat override (otherwise the model's materials)
    glm::vec4 tint{1.0f};
    bool castShadows = true;
    bool visible = true;
    RenderLayer layer = RenderLayer::World;

    // Runtime cache (resolved lazily by the renderer, never serialized)
    Ref<Model> model;
    Ref<Material> materialOverride;
    std::string loadedMesh;
    std::string loadedMaterial;
    bool loadFailed = false;
};

enum class LightType : int { Directional = 0, Point = 1, Spot = 2 };

struct Light {
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;        // point/spot
    float spotAngle = 45.0f;    // spot, full cone angle in degrees
    bool castShadows = true;    // directional only
};

struct BoxCollider {
    glm::vec3 center{0.0f};
    glm::vec3 size{1.0f};
    bool isTrigger = false;
};

// Simple dynamic body: gravity + drag + axis-aligned collision response.
struct RigidBody {
    float mass = 1.0f;
    float drag = 0.2f;
    float bounciness = 0.2f;
    bool useGravity = true;
    bool isKinematic = false;
    glm::vec3 velocity{0.0f};
    bool grounded = false;
};

// Axis-aligned capsule-ish box moved by gameplay code through PhysicsWorld::moveBox.
struct CharacterController {
    float height = 1.8f;
    float radius = 0.35f;
    glm::vec3 velocity{0.0f};
    bool grounded = false;
};

struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float age = 0.0f;
    float lifetime = 1.0f;
};

struct ParticleEmitter {
    bool emitting = true;
    bool loop = true;
    float rate = 20.0f;            // particles per second
    int burst = 0;                 // particles emitted once when the emitter starts
    int maxParticles = 256;
    float lifetime = 1.0f;
    float startSpeed = 3.0f;
    float spread = 0.3f;           // 0 = straight line, 1 = full sphere
    glm::vec3 direction{0.0f, 1.0f, 0.0f};
    float gravity = 0.0f;          // multiplier of world gravity
    float drag = 0.0f;
    float startSize = 0.2f;
    float endSize = 0.0f;
    glm::vec4 startColor{1.0f, 0.8f, 0.4f, 1.0f};
    glm::vec4 endColor{1.0f, 0.2f, 0.0f, 0.0f};

    // Runtime
    std::vector<Particle> particles;
    float emitAccumulator = 0.0f;
    bool burstDone = false;
};

// Destroys the entity after `seconds` (play mode only).
struct Lifetime {
    float seconds = 1.0f;
};

// Registers the built-in components with the ComponentRegistry (idempotent).
void registerCoreComponents();

} // namespace Forge
