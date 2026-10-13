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
        .field("Aim Move Multiplier", &Player::aimMoveMultiplier, 0.01f, 0.1f, 1.0f)
        .entity("Camera", &Player::camera).tooltip("Child camera rotated for pitch")
        .entity("Weapon", &Player::weapon).tooltip("Equipped weapon. Every Weapon entity under the camera is part of the inventory");

    r.add<Health>("Health", "Gameplay")
        .field("Current", &Health::current, 0.5f, 0.0f, 1000.0f)
        .field("Max", &Health::max, 0.5f, 1.0f, 1000.0f);

    r.add<Weapon>("Weapon", "Gameplay")
        .field("Slot", &Weapon::slot, 1.0f, 1.0f, 9.0f).tooltip("Number key that selects the weapon")
        .enumeration("Kind", &Weapon::kind, {"Hitscan", "Launcher"})
        .field("Automatic", &Weapon::automatic)
        .field("Damage", &Weapon::damage, 0.5f, 0.0f, 500.0f).tooltip("Per pellet (hitscan) or at the explosion center (launcher)")
        .field("Pellets", &Weapon::pellets, 1.0f, 1.0f, 32.0f)
        .field("Fire Rate", &Weapon::fireRate, 0.1f, 0.1f, 30.0f).tooltip("Rounds per second")
        .field("Range", &Weapon::range, 1.0f)
        .field("Spread", &Weapon::spread, 0.01f, 0.0f, 20.0f).tooltip("Degrees")
        .field("Recoil", &Weapon::recoil, 0.01f, 0.0f, 20.0f)
        .field("Magazine Size", &Weapon::magazineSize, 1.0f, 1.0f, 200.0f)
        .field("Ammo", &Weapon::ammo, 1.0f, 0.0f, 200.0f)
        .field("Reserve Ammo", &Weapon::reserveAmmo, 1.0f)
        .field("Max Reserve", &Weapon::maxReserve, 1.0f, 0.0f, 9999.0f)
        .field("Ammo Per Pickup", &Weapon::ammoPerPickup, 1.0f, 0.0f, 999.0f)
        .field("Reload Time", &Weapon::reloadTime, 0.01f, 0.1f, 10.0f)
        .field("Equip Time", &Weapon::equipTime, 0.01f, 0.0f, 3.0f)
        .field("Impact Force", &Weapon::impactForce, 0.1f, 0.0f, 100.0f)
        .field("Aim FOV", &Weapon::aimFov, 0.1f, 5.0f, 120.0f).tooltip("Camera field of view while aiming down sights")
        .field("Aim Position", &Weapon::aimPosition, 0.002f).tooltip("Viewmodel position that lines the sights up with the screen center")
        .field("Aim Speed", &Weapon::aimSpeed, 0.1f, 1.0f, 40.0f)
        .field("Aim Spread", &Weapon::aimSpread, 0.01f, 0.0f, 1.0f).tooltip("Spread multiplier while aiming")
        .field("Scope", &Weapon::scope).tooltip("Show a scope overlay and hide the viewmodel when fully aimed")
        .field("Projectile Speed", &Weapon::projectileSpeed, 0.1f, 1.0f, 300.0f)
        .field("Explosion Radius", &Weapon::explosionRadius, 0.05f, 0.0f, 50.0f)
        .entity("Muzzle Flash", &Weapon::muzzleFlash)
        .field("Rest Position", &Weapon::restPosition, 0.005f);

    r.add<Explosive>("Explosive", "Gameplay")
        .field("Radius", &Explosive::radius, 0.05f, 0.0f, 50.0f)
        .field("Damage", &Explosive::damage, 0.5f, 0.0f, 1000.0f).tooltip("Damage at the center, falling off towards the radius")
        .field("Force", &Explosive::force, 0.1f, 0.0f, 200.0f)
        .field("Ignite Below", &Explosive::igniteBelow, 0.01f, 0.0f, 1.0f).tooltip("Health fraction below which it starts burning")
        .field("Burn Rate", &Explosive::burnRate, 0.1f, 0.0f, 500.0f).tooltip("Health lost per second while burning")
        .field("Respawn Time", &Explosive::respawnTime, 0.1f, 0.0f, 600.0f).tooltip("0 = never respawns")
        .entity("Fire", &Explosive::fire).tooltip("Child shown while burning (particles + light)");

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
        .entity("Owner", &Projectile::owner)
        .field("Explosion Radius", &Projectile::explosionRadius, 0.05f, 0.0f, 50.0f);

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
