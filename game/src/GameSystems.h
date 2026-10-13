#pragma once
// Gameplay systems. Each one iterates the components it cares about every frame.
#include "GameComponents.h"

namespace FPS {

// Waves, scoring, game over and restart.
class GameManagerSystem : public Forge::System {
public:
    GameManagerSystem() { priority = -10; }
    const char* name() const override { return "GameManagerSystem"; }
    void onStart(Forge::Scene& scene) override;
    void onUpdate(Forge::Scene& scene, float dt) override;

    static void restart(Forge::Scene& scene);
};

// Mouse look, walking, sprinting, jumping, head bob and death camera.
class PlayerSystem : public Forge::System {
public:
    const char* name() const override { return "PlayerSystem"; }
    void onStart(Forge::Scene& scene) override;
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Hitscan rifle: firing, spread, recoil, reloading, muzzle flash and viewmodel animation.
class WeaponSystem : public Forge::System {
public:
    const char* name() const override { return "WeaponSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Hovering drones: chase/strafe around the player, shoot plasma bolts, explode on death.
class EnemySystem : public Forge::System {
public:
    const char* name() const override { return "EnemySystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Moves plasma bolts and resolves their hits (swept raycasts).
class ProjectileSystem : public Forge::System {
public:
    const char* name() const override { return "ProjectileSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Health / ammo pickups with respawn timers.
class PickupSystem : public Forge::System {
public:
    const char* name() const override { return "PickupSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Rotates / bobs decorative entities.
class SpinnerSystem : public Forge::System {
public:
    const char* name() const override { return "SpinnerSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Helpers shared by systems and the HUD
Entity findPlayer(Forge::Scene& scene);
GameManager* findGameManager(Forge::Scene& scene);

} // namespace FPS
