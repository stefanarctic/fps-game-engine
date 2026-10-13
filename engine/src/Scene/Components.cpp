#include "Forge/Scene/Components.h"
#include "Forge/ECS/Reflection.h"

namespace Forge {

void registerCoreComponents() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    ComponentRegistry& r = ComponentRegistry::instance();

    r.add<Transform>("Transform", "Core")
        .removable(false)
        .field("Position", &Transform::position, 0.05f)
        .field("Rotation", &Transform::rotation, 0.5f)
        .field("Scale", &Transform::scale, 0.02f);

    r.add<Camera>("Camera", "Rendering")
        .field("Field Of View", &Camera::fieldOfView, 0.5f, 1.0f, 179.0f)
        .field("Near Clip", &Camera::nearClip, 0.01f)
        .field("Far Clip", &Camera::farClip, 1.0f)
        .field("Primary", &Camera::primary).tooltip("The Game view renders from the first primary camera")
        .enumeration("Clear Flags", &Camera::clear, {"Skybox", "Solid Color"})
        .color("Background", &Camera::background);

    r.add<MeshRenderer>("Mesh Renderer", "Rendering")
        .asset("Mesh", &MeshRenderer::mesh, AssetKind::Model)
        .asset("Material", &MeshRenderer::material, AssetKind::Material).tooltip("Overrides the model's own materials")
        .color("Tint", &MeshRenderer::tint)
        .field("Cast Shadows", &MeshRenderer::castShadows)
        .field("Visible", &MeshRenderer::visible)
        .enumeration("Layer", &MeshRenderer::layer, {"World", "Viewmodel"}).tooltip("Viewmodel renders on top of the world (first-person weapons)");

    r.add<Light>("Light", "Rendering")
        .enumeration("Type", &Light::type, {"Directional", "Point", "Spot"})
        .color("Color", &Light::color)
        .field("Intensity", &Light::intensity, 0.02f, 0.0f, 50.0f)
        .field("Range", &Light::range, 0.1f)
        .field("Spot Angle", &Light::spotAngle, 0.5f, 1.0f, 179.0f)
        .field("Cast Shadows", &Light::castShadows);

    r.add<BoxCollider>("Box Collider", "Physics")
        .field("Center", &BoxCollider::center, 0.02f)
        .field("Size", &BoxCollider::size, 0.02f)
        .field("Is Trigger", &BoxCollider::isTrigger);

    r.add<RigidBody>("Rigidbody", "Physics")
        .field("Mass", &RigidBody::mass, 0.05f, 0.01f, 1000.0f)
        .field("Drag", &RigidBody::drag, 0.01f, 0.0f, 10.0f)
        .field("Bounciness", &RigidBody::bounciness, 0.01f, 0.0f, 1.0f)
        .field("Use Gravity", &RigidBody::useGravity)
        .field("Is Kinematic", &RigidBody::isKinematic);

    r.add<CharacterController>("Character Controller", "Physics")
        .field("Height", &CharacterController::height, 0.01f, 0.1f, 10.0f)
        .field("Radius", &CharacterController::radius, 0.01f, 0.05f, 5.0f);

    r.add<ParticleEmitter>("Particle System", "Effects")
        .field("Emitting", &ParticleEmitter::emitting)
        .field("Looping", &ParticleEmitter::loop)
        .field("Rate", &ParticleEmitter::rate, 0.5f, 0.0f, 1000.0f)
        .field("Burst", &ParticleEmitter::burst, 1.0f, 0.0f, 1000.0f)
        .field("Max Particles", &ParticleEmitter::maxParticles, 1.0f, 1.0f, 10000.0f)
        .field("Lifetime", &ParticleEmitter::lifetime, 0.01f, 0.01f, 30.0f)
        .field("Start Speed", &ParticleEmitter::startSpeed, 0.05f)
        .field("Spread", &ParticleEmitter::spread, 0.01f, 0.0f, 1.0f)
        .field("Direction", &ParticleEmitter::direction, 0.01f)
        .field("Gravity", &ParticleEmitter::gravity, 0.01f)
        .field("Drag", &ParticleEmitter::drag, 0.01f, 0.0f, 20.0f)
        .field("Start Size", &ParticleEmitter::startSize, 0.01f, 0.0f, 50.0f)
        .field("End Size", &ParticleEmitter::endSize, 0.01f, 0.0f, 50.0f)
        .color("Start Color", &ParticleEmitter::startColor, true)
        .color("End Color", &ParticleEmitter::endColor, true);

    r.add<Lifetime>("Lifetime", "Misc")
        .field("Seconds", &Lifetime::seconds, 0.05f, 0.0f, 3600.0f);
}

} // namespace Forge
