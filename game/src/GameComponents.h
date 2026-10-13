#pragma once
// Gameplay components for the FPS. Pure data: all behaviour lives in GameSystems.
#include <Forge/Forge.h>

namespace FPS {

using Forge::Entity;
using Forge::NullEntity;

struct Player {
    float moveSpeed = 6.0f;
    float sprintMultiplier = 1.6f;
    float jumpSpeed = 7.5f;
    float mouseSensitivity = 0.12f;
    float eyeHeight = 0.7f;            // camera offset above the body center
    float aimMoveMultiplier = 0.6f;    // walk speed while aiming down sights
    Entity camera = NullEntity;        // child camera (pitch)
    Entity weapon = NullEntity;        // equipped weapon; every Weapon under the camera is in the inventory

    // Runtime
    float yaw = 0.0f;
    float pitch = 0.0f;
    float recoil = 0.0f;               // temporary pitch kick (degrees)
    float bobPhase = 0.0f;
    float bobAmount = 0.0f;
    float damageFlash = 0.0f;
    float shake = 0.0f;                // camera shake from explosions (0..1)
    bool dead = false;
    bool sprinting = false;
    bool aimToggled = false;           // toggle-aim mode state
    Entity previousWeapon = NullEntity;
    float switchBanner = 0.0f;         // HUD weapon-name banner timer
    glm::vec3 spawnPosition{0.0f};
    float spawnYaw = 0.0f;
};

struct Health {
    float current = 100.0f;
    float max = 100.0f;
};

// Hitscan weapons trace rays (rifle, shotgun, sniper); launchers fire explosive rockets.
enum class WeaponKind : int { Hitscan = 0, Launcher = 1 };

struct Weapon {
    int slot = 1;                      // number key that selects it, also the inventory order
    WeaponKind kind = WeaponKind::Hitscan;
    bool automatic = true;             // hold to fire; otherwise one shot per click
    float damage = 22.0f;              // per pellet / per rocket explosion
    int pellets = 1;                   // rays per shot (shotgun)
    float fireRate = 9.0f;             // rounds per second
    float range = 200.0f;
    float spread = 0.6f;               // base spread (degrees)
    float recoil = 0.9f;               // pitch kick per shot (degrees)
    int magazineSize = 30;
    int ammo = 30;
    int reserveAmmo = 150;
    int maxReserve = 300;
    int ammoPerPickup = 60;            // rounds granted by an ammo box
    float reloadTime = 1.5f;
    float equipTime = 0.35f;
    float impactForce = 7.0f;

    // Aiming down sights / scope
    float aimFov = 52.0f;              // camera field of view when fully aimed
    glm::vec3 aimPosition{0.0f, -0.07f, -0.32f};
    float aimSpeed = 10.0f;            // blend speed into/out of aiming
    float aimSpread = 0.3f;            // spread multiplier while aiming
    bool scope = false;                // full-screen scope overlay (hides the viewmodel)

    // Launcher
    float projectileSpeed = 32.0f;
    float explosionRadius = 5.0f;

    Entity muzzleFlash = NullEntity;
    glm::vec3 restPosition{0.16f, -0.15f, -0.5f};

    // Runtime
    float cooldown = 0.0f;
    float reloadTimer = 0.0f;
    bool reloading = false;
    float bloom = 0.0f;                // extra spread from sustained fire
    float flashTimer = 0.0f;
    float kick = 0.0f;
    float aim = 0.0f;                  // 0 = hip, 1 = fully aimed
    float equipTimer = 0.0f;
    float sprint = 0.0f;               // sprint pose blend
    float queuedShot = 0.0f;           // semi-auto click buffered during the cooldown
    int initialReserve = -1;           // reserve at the start of play (restored on restart)
    glm::vec2 sway{0.0f};
};

// Barrels and other props that burn when damaged and explode when destroyed,
// damaging everything around them (including other explosives: chain reactions).
struct Explosive {
    float radius = 5.5f;
    float damage = 120.0f;
    float force = 14.0f;               // impulse given to rigid bodies
    float igniteBelow = 0.6f;          // starts burning below this fraction of its health
    float burnRate = 22.0f;            // health lost per second while burning
    float respawnTime = 40.0f;         // 0 = never comes back
    Entity fire = NullEntity;          // child with the fire particles + light, shown while burning

    // Runtime
    bool burning = false;
    bool exploded = false;
    float fuse = -1.0f;                // delay before a chain-reaction detonation
    float respawnTimer = 0.0f;
};

struct Enemy {
    float speed = 4.5f;
    float preferredDistance = 9.0f;
    float attackRange = 30.0f;
    float fireInterval = 1.7f;
    float projectileSpeed = 20.0f;
    float damage = 9.0f;
    int scoreValue = 100;

    // Runtime
    glm::vec3 velocity{0.0f};
    float fireTimer = 1.0f;
    float strafeTimer = 0.0f;
    float strafeDir = 1.0f;
    float phase = 0.0f;
    float hitFlash = 0.0f;
};

struct Projectile {
    float damage = 9.0f;
    glm::vec3 velocity{0.0f};
    Entity owner = NullEntity;
    float explosionRadius = 0.0f;      // > 0: explodes on impact (rockets)
};

enum class PickupType : int { Health = 0, Ammo = 1 };

struct Pickup {
    PickupType type = PickupType::Health;
    float amount = 35.0f;              // health points, or ammo boxes (x each weapon's Ammo Per Pickup)
    float respawnTime = 15.0f;

    // Runtime
    bool available = true;
    float timer = 0.0f;
};

// Simple decorative animation (pickups, fans, beacons).
struct Spinner {
    glm::vec3 speed{0.0f, 90.0f, 0.0f}; // degrees per second
    float bobAmplitude = 0.0f;
    float bobFrequency = 2.0f;
    float baseY = 0.0f;
    bool initialized = false;
};

struct SpawnPoint {
    float radius = 1.5f;
};

struct GameManager {
    float timeBetweenWaves = 4.0f;
    int baseEnemies = 3;
    int enemiesPerWave = 2;
    int maxAlive = 10;

    // Runtime
    int wave = 0;
    int score = 0;
    int kills = 0;
    int toSpawn = 0;
    float waveTimer = 3.0f;
    float spawnTimer = 0.0f;
    float hitMarker = 0.0f;
    float messageTimer = 0.0f;
    std::string message;
    std::string toast;                 // small pickup notification
    float toastTimer = 0.0f;
    bool gameOver = false;
    float gameOverTimer = 0.0f;
};

void registerGameComponents();

} // namespace FPS
