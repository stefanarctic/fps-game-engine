#include "GameComponents.h"

namespace FPS {

using namespace Forge;

void registerGameComponents() {
    ComponentRegistry& r = ComponentRegistry::instance();

    r.add<Player>("Player", "Gameplay")
        .field("Move Speed", &Player::moveSpeed, 0.05f, 0.0f, 30.0f)
        .field("Sprint Multiplier", &Player::sprintMultiplier, 0.01f, 1.0f, 4.0f)
        .field("Jump Speed", &Player::jumpSpeed, 0.05f, 0.0f, 30.0f)
        .field("Mouse Sensitivity", &Player::mouseSensitivity, 0.005f, 0.01f, 1.0f)
        .field("Eye Height", &Player::eyeHeight, 0.01f, 0.0f, 2.0f)
        .entity("Camera", &Player::camera).tooltip("Child camera rotated for pitch")
        .entity("Weapon", &Player::weapon).tooltip("Viewmodel entity with a Weapon component");

    r.add<Health>("Health", "Gameplay")
        .field("Current", &Health::current, 0.5f, 0.0f, 1000.0f)
        .field("Max", &Health::max, 0.5f, 1.0f, 1000.0f);

    r.add<Weapon>("Weapon", "Gameplay")
        .field("Damage", &Weapon::damage, 0.5f, 0.0f, 500.0f)
        .field("Fire Rate", &Weapon::fireRate, 0.1f, 0.5f, 30.0f).tooltip("Rounds per second")
        .field("Range", &Weapon::range, 1.0f)
        .field("Spread", &Weapon::spread, 0.01f, 0.0f, 10.0f).tooltip("Degrees")
        .field("Recoil", &Weapon::recoil, 0.01f, 0.0f, 10.0f)
        .field("Magazine Size", &Weapon::magazineSize, 1.0f, 1.0f, 200.0f)
        .field("Ammo", &Weapon::ammo, 1.0f, 0.0f, 200.0f)
        .field("Reserve Ammo", &Weapon::reserveAmmo, 1.0f)
        .field("Reload Time", &Weapon::reloadTime, 0.01f, 0.1f, 10.0f)
        .field("Impact Force", &Weapon::impactForce, 0.1f, 0.0f, 100.0f)
        .entity("Muzzle Flash", &Weapon::muzzleFlash)
        .field("Rest Position", &Weapon::restPosition, 0.005f);

    r.add<Enemy>("Enemy", "Gameplay")
        .field("Speed", &Enemy::speed, 0.05f, 0.0f, 30.0f)
        .field("Preferred Distance", &Enemy::preferredDistance, 0.1f, 0.0f, 100.0f)
        .field("Attack Range", &Enemy::attackRange, 0.1f, 0.0f, 200.0f)
        .field("Fire Interval", &Enemy::fireInterval, 0.01f, 0.1f, 20.0f)
        .field("Projectile Speed", &Enemy::projectileSpeed, 0.1f, 1.0f, 200.0f)
        .field("Damage", &Enemy::damage, 0.1f, 0.0f, 200.0f)
        .field("Score Value", &Enemy::scoreValue, 1.0f, 0.0f, 10000.0f);

    r.add<Projectile>("Projectile", "Gameplay")
        .field("Damage", &Projectile::damage, 0.1f)
        .field("Velocity", &Projectile::velocity, 0.1f)
        .entity("Owner", &Projectile::owner);

    r.add<Pickup>("Pickup", "Gameplay")
        .enumeration("Type", &Pickup::type, {"Health", "Ammo"})
        .field("Amount", &Pickup::amount, 0.5f, 0.0f, 1000.0f)
        .field("Respawn Time", &Pickup::respawnTime, 0.1f, 0.0f, 600.0f);

    r.add<Spinner>("Spinner", "Gameplay")
        .field("Speed", &Spinner::speed, 1.0f)
        .field("Bob Amplitude", &Spinner::bobAmplitude, 0.005f, 0.0f, 5.0f)
        .field("Bob Frequency", &Spinner::bobFrequency, 0.01f, 0.0f, 20.0f);

    r.add<SpawnPoint>("Spawn Point", "Gameplay")
        .field("Radius", &SpawnPoint::radius, 0.05f, 0.0f, 20.0f);

    r.add<GameManager>("Game Manager", "Gameplay")
        .field("Time Between Waves", &GameManager::timeBetweenWaves, 0.05f, 0.0f, 60.0f)
        .field("Base Enemies", &GameManager::baseEnemies, 1.0f, 0.0f, 50.0f)
        .field("Enemies Per Wave", &GameManager::enemiesPerWave, 1.0f, 0.0f, 50.0f)
        .field("Max Alive", &GameManager::maxAlive, 1.0f, 1.0f, 100.0f);
}

} // namespace FPS
