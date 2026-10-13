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
    Entity camera = NullEntity;        // child camera (pitch)
    Entity weapon = NullEntity;        // viewmodel weapon (child of the camera)

    // Runtime
    float yaw = 0.0f;
    float pitch = 0.0f;
    float recoil = 0.0f;               // temporary pitch kick (degrees)
    float bobPhase = 0.0f;
    float bobAmount = 0.0f;
    float damageFlash = 0.0f;
    bool dead = false;
    glm::vec3 spawnPosition{0.0f};
    float spawnYaw = 0.0f;
};

struct Health {
    float current = 100.0f;
    float max = 100.0f;
};

struct Weapon {
    float damage = 22.0f;
    float fireRate = 9.0f;             // rounds per second
    float range = 200.0f;
    float spread = 0.6f;               // base spread (degrees)
    float recoil = 0.9f;               // pitch kick per shot (degrees)
    int magazineSize = 30;
    int ammo = 30;
    int reserveAmmo = 150;
    float reloadTime = 1.5f;
    float impactForce = 7.0f;
    Entity muzzleFlash = NullEntity;
    glm::vec3 restPosition{0.16f, -0.15f, -0.5f};

    // Runtime
    float cooldown = 0.0f;
    float reloadTimer = 0.0f;
    bool reloading = false;
    float bloom = 0.0f;                // extra spread from sustained fire
    float flashTimer = 0.0f;
    float kick = 0.0f;
    glm::vec2 sway{0.0f};
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
};

enum class PickupType : int { Health = 0, Ammo = 1 };

struct Pickup {
    PickupType type = PickupType::Health;
    float amount = 35.0f;
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
