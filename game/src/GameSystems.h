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

// Mouse look, walking, sprinting, jumping, head bob, camera shake and death camera.
class PlayerSystem : public Forge::System {
public:
    const char* name() const override { return "PlayerSystem"; }
    void onStart(Forge::Scene& scene) override;
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Weapon inventory and switching, aiming down sights / scopes, hitscan and
// rocket firing, spread, recoil, reloading, muzzle flash and viewmodel animation.
class WeaponSystem : public Forge::System {
public:
    const char* name() const override { return "WeaponSystem"; }
    void onStart(Forge::Scene& scene) override;
    void onUpdate(Forge::Scene& scene, float dt) override;

    // Makes `weapon` the equipped one (plays the raise animation).
    static void equip(Forge::Scene& scene, Player& player, Entity weapon);
};

// Hovering drones: chase/strafe around the player, shoot plasma bolts, explode on death.
class EnemySystem : public Forge::System {
public:
    const char* name() const override { return "EnemySystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Moves plasma bolts and rockets and resolves their hits (swept raycasts).
class ProjectileSystem : public Forge::System {
public:
    const char* name() const override { return "ProjectileSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;
};

// Explosive props: ignite when damaged, burn down, detonate (with chain reactions) and respawn.
class ExplosiveSystem : public Forge::System {
public:
    const char* name() const override { return "ExplosiveSystem"; }
    void onUpdate(Forge::Scene& scene, float dt) override;

    static void reset(Forge::Scene& scene);
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
// The player's weapons (Weapon entities directly under its camera), sorted by slot.
std::vector<Entity> weaponInventory(Forge::Scene& scene, const Player& player);
// The equipped weapon component, or null.
Weapon* equippedWeapon(Forge::Scene& scene, const Player& player);

} // namespace FPS
