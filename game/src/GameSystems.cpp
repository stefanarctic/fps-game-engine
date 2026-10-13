#include "GameSystems.h"
#include "Combat.h"
#include "Prefabs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace FPS {

using namespace Forge;

Entity findPlayer(Scene& scene) {
    Entity found = NullEntity;
    scene.each<Player>([&](Entity e, Player&) {
        if (found == NullEntity) found = e;
    });
    return found;
}

GameManager* findGameManager(Scene& scene) {
    GameManager* gm = nullptr;
    scene.each<GameManager>([&](Entity, GameManager& g) {
        if (!gm) gm = &g;
    });
    return gm;
}

std::vector<Entity> weaponInventory(Scene& scene, const Player& player) {
    std::vector<Entity> out;
    if (!scene.valid(player.camera)) return out;
    for (Entity c : scene.get<Transform>(player.camera).children)
        if (scene.valid(c) && scene.has<Weapon>(c)) out.push_back(c);
    std::stable_sort(out.begin(), out.end(), [&](Entity a, Entity b) { return scene.get<Weapon>(a).slot < scene.get<Weapon>(b).slot; });
    return out;
}

Weapon* equippedWeapon(Scene& scene, const Player& player) {
    return scene.valid(player.weapon) ? scene.tryGet<Weapon>(player.weapon) : nullptr;
}

// ---------------------------------------------------------------------------
// Game manager: waves, score, game over
// ---------------------------------------------------------------------------
void GameManagerSystem::onStart(Scene& scene) {
    GameManager* gm = findGameManager(scene);
    if (!gm) {
        LOG_WARN("No Game Manager in the scene: enemy waves are disabled");
        return;
    }
    gm->wave = 0;
    gm->score = 0;
    gm->kills = 0;
    gm->toSpawn = 0;
    gm->waveTimer = 2.5f;
    gm->spawnTimer = 0.0f;
    gm->gameOver = false;
    gm->message = "GET READY";
    gm->messageTimer = 2.5f;
}

void GameManagerSystem::restart(Scene& scene) {
    for (Entity e : scene.registry().view<Enemy>()) scene.destroyEntity(e);
    for (Entity e : scene.registry().view<Projectile>()) scene.destroyEntity(e);

    Entity pe = findPlayer(scene);
    if (scene.valid(pe)) {
        Player& p = scene.get<Player>(pe);
        Transform& t = scene.get<Transform>(pe);
        p.dead = false;
        p.yaw = p.spawnYaw;
        p.pitch = 0.0f;
        p.recoil = 0.0f;
        p.damageFlash = 0.0f;
        p.shake = 0.0f;
        t.position = p.spawnPosition;
        if (Health* hp = scene.tryGet<Health>(pe)) hp->current = hp->max;
        if (CharacterController* cc = scene.tryGet<CharacterController>(pe)) cc->velocity = glm::vec3(0.0f);
        if (scene.valid(p.camera)) {
            scene.get<Transform>(p.camera).position = glm::vec3(0.0f, p.eyeHeight, 0.0f);
            scene.get<Transform>(p.camera).rotation = glm::vec3(0.0f);
        }
        for (Entity we : weaponInventory(scene, p)) {
            Weapon& w = scene.get<Weapon>(we);
            w.ammo = w.magazineSize;
            if (w.initialReserve >= 0) w.reserveAmmo = w.initialReserve;
            w.reloading = false;
            w.aim = 0.0f;
        }
    }
    scene.each<Pickup, EntityInfo>([](Entity, Pickup& pk, EntityInfo& info) {
        pk.available = true;
        info.active = true;
    });
    ExplosiveSystem::reset(scene);
    if (GameManager* gm = findGameManager(scene)) {
        gm->wave = 0;
        gm->score = 0;
        gm->kills = 0;
        gm->toSpawn = 0;
        gm->waveTimer = 2.0f;
        gm->gameOver = false;
        gm->message = "GET READY";
        gm->messageTimer = 2.0f;
    }
    LOG_INFO("Game restarted");
}

void GameManagerSystem::onUpdate(Scene& scene, float dt) {
    GameManager* gm = findGameManager(scene);
    if (!gm) return;
    gm->hitMarker -= dt;
    gm->messageTimer -= dt;
    gm->toastTimer -= dt;

    Entity pe = findPlayer(scene);
    Health* hp = scene.valid(pe) ? scene.tryGet<Health>(pe) : nullptr;
    if (!gm->gameOver && hp && hp->current <= 0.0f) {
        gm->gameOver = true;
        gm->gameOverTimer = 0.0f;
        LOG_INFO("Player died on wave %d with %d points", gm->wave, gm->score);
    }
    if (gm->gameOver) {
        gm->gameOverTimer += dt;
        if (Input::getKeyDown(Key::R) || (gm->gameOverTimer > 1.5f && Input::getMouseButtonDown(Mouse::Left))) restart(scene);
        return;
    }

    int alive = (int)scene.registry().count<Enemy>();
    if (gm->toSpawn > 0) {
        gm->spawnTimer -= dt;
        if (gm->spawnTimer <= 0.0f && alive < gm->maxAlive) {
            std::vector<Entity> points = scene.registry().view<SpawnPoint, Transform>();
            if (!points.empty()) {
                Entity sp = points[(size_t)Random::range(0, (int)points.size())];
                float radius = scene.get<SpawnPoint>(sp).radius;
                glm::vec3 pos = scene.get<Transform>(sp).worldPosition() +
                                glm::vec3(Random::range(-radius, radius), 0.0f, Random::range(-radius, radius));
                Prefabs::drone(scene, pos);
                Prefabs::impact(scene, pos, glm::vec3(0, 1, 0), glm::vec4(0.6f, 1.5f, 3.0f, 1.0f));
            }
            gm->toSpawn--;
            gm->spawnTimer = 0.7f;
        }
    } else if (alive == 0) {
        gm->waveTimer -= dt;
        if (gm->waveTimer <= 0.0f) {
            gm->wave++;
            gm->toSpawn = gm->baseEnemies + (gm->wave - 1) * gm->enemiesPerWave;
            gm->message = "WAVE " + std::to_string(gm->wave);
            gm->messageTimer = 2.5f;
            gm->waveTimer = gm->timeBetweenWaves;
            gm->spawnTimer = 0.5f;
            LOG_INFO("Wave %d: %d drones incoming", gm->wave, gm->toSpawn);
        }
    }
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
void PlayerSystem::onStart(Scene& scene) {
    scene.each<Player, Transform>([](Entity, Player& p, Transform& t) {
        p.yaw = t.rotation.y;
        p.pitch = 0.0f;
        p.dead = false;
        p.spawnPosition = t.position;
        p.spawnYaw = t.rotation.y;
    });
}

void PlayerSystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();
    const GameSettings& settings = Settings::get();
    scene.each<Player, CharacterController, Transform>([&](Entity e, Player& p, CharacterController& cc, Transform& t) {
        Health* hp = scene.tryGet<Health>(e);
        if (hp && hp->current <= 0.0f) p.dead = true;
        p.damageFlash = std::max(0.0f, p.damageFlash - dt * 1.5f);
        p.shake = std::max(0.0f, p.shake - dt * 1.6f);
        p.switchBanner = std::max(0.0f, p.switchBanner - dt);

        Weapon* weapon = equippedWeapon(scene, p);
        float aim = weapon ? weapon->aim : 0.0f;

        // Mouse look. Sensitivity follows the zoom so aiming feels consistent.
        if (!p.dead) {
            float zoom = 1.0f;
            if (scene.valid(p.camera))
                if (const Camera* cam = scene.tryGet<Camera>(p.camera))
                    zoom = std::tan(glm::radians(cam->fieldOfView) * 0.5f) / std::tan(glm::radians(settings.fieldOfView) * 0.5f);
            float sensitivity = p.mouseSensitivity * settings.mouseSensitivity * glm::clamp(zoom, 0.05f, 1.0f);
            glm::vec2 md = Input::mouseDelta();
            p.yaw -= md.x * sensitivity;
            p.pitch = glm::clamp(p.pitch - md.y * sensitivity * (settings.invertY ? -1.0f : 1.0f), -88.0f, 88.0f);
        }
        p.recoil = Math::lerp(p.recoil, 0.0f, Math::damp(7.0f, dt));
        t.rotation = glm::vec3(0.0f, p.yaw, 0.0f);

        // Walk / sprint (no sprinting while aiming or shooting)
        float yawRad = glm::radians(p.yaw);
        glm::vec3 forward(-std::sin(yawRad), 0.0f, -std::cos(yawRad));
        glm::vec3 right(std::cos(yawRad), 0.0f, -std::sin(yawRad));
        glm::vec3 wish(0.0f);
        if (!p.dead) {
            if (Input::getKey(Key::W)) wish += forward;
            if (Input::getKey(Key::S)) wish -= forward;
            if (Input::getKey(Key::D)) wish += right;
            if (Input::getKey(Key::A)) wish -= right;
        }
        if (glm::dot(wish, wish) > 0.0f) wish = glm::normalize(wish);
        p.sprinting = Input::getKey(Key::LeftShift) && glm::dot(wish, forward) > 0.5f && aim < 0.3f && !Input::getMouseButton(Mouse::Left);
        float speed = p.moveSpeed * (p.sprinting ? p.sprintMultiplier : 1.0f) * Math::lerp(1.0f, p.aimMoveMultiplier, aim);
        glm::vec3 target = wish * speed;
        float accel = cc.grounded ? 14.0f : 3.0f;
        float k = Math::damp(accel, dt);
        cc.velocity.x = Math::lerp(cc.velocity.x, target.x, k);
        cc.velocity.z = Math::lerp(cc.velocity.z, target.z, k);

        // Gravity / jump
        cc.velocity.y += physics.gravity.y * dt;
        if (cc.grounded && !p.dead && Input::getKeyDown(Key::Space)) {
            cc.velocity.y = p.jumpSpeed;
            cc.grounded = false;
        }

        glm::vec3 half(cc.radius, cc.height * 0.5f, cc.radius);
        MoveResult res = physics.moveBox(t.position, half, cc.velocity * dt, e);
        cc.grounded = res.grounded;
        if (res.grounded && cc.velocity.y < 0.0f) cc.velocity.y = 0.0f;
        if (res.hitCeiling && cc.velocity.y > 0.0f) cc.velocity.y = 0.0f;
        if (res.blocked.x) cc.velocity.x = 0.0f;
        if (res.blocked.z) cc.velocity.z = 0.0f;
        t.position = res.position;

        if (t.position.y < -30.0f) {
            t.position = p.spawnPosition;
            cc.velocity = glm::vec3(0.0f);
        }

        // Head bob (much smaller while aiming)
        float horizontal = glm::length(glm::vec2(cc.velocity.x, cc.velocity.z));
        float targetBob = (cc.grounded && horizontal > 0.5f) ? std::min(horizontal / p.moveSpeed, 1.6f) : 0.0f;
        p.bobAmount = Math::lerp(p.bobAmount, targetBob * (1.0f - 0.8f * aim), Math::damp(10.0f, dt));
        p.bobPhase += dt * (5.0f + horizontal * 0.9f);

        if (scene.valid(p.camera)) {
            Transform& ct = scene.get<Transform>(p.camera);
            if (p.dead) {
                // Collapse to the floor
                float s = Math::damp(3.0f, dt);
                ct.position = glm::mix(ct.position, glm::vec3(0.0f, -0.55f, 0.0f), s);
                ct.rotation = glm::mix(ct.rotation, glm::vec3(p.pitch * 0.3f, 0.0f, 40.0f), s);
            } else {
                float bobY = std::sin(p.bobPhase * 2.0f) * 0.035f * p.bobAmount;
                float bobX = std::cos(p.bobPhase) * 0.025f * p.bobAmount;
                ct.position = glm::vec3(bobX, p.eyeHeight + bobY, 0.0f);
                glm::vec3 shake(0.0f);
                if (p.shake > 0.0f) {
                    float s = p.shake * p.shake * 3.0f;
                    shake = glm::vec3(Random::range(-s, s), Random::range(-s, s), Random::range(-s, s) * 0.5f);
                }
                ct.rotation = glm::vec3(p.pitch + p.recoil, 0.0f, 0.0f) + shake;
            }
        }
    });
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------
namespace {
// Random direction inside a cone of `degrees` around `forward`.
glm::vec3 coneDirection(const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up, float degrees) {
    float a = glm::radians(degrees) * std::sqrt(Random::value());
    float theta = Random::range(0.0f, 2.0f * Math::PI);
    return glm::normalize(forward + (right * std::cos(theta) + up * std::sin(theta)) * std::tan(a));
}

float smooth(float t) { return t * t * (3.0f - 2.0f * t); }
} // namespace

void WeaponSystem::onStart(Scene& scene) {
    scene.each<Player>([&](Entity, Player& p) {
        std::vector<Entity> inventory = weaponInventory(scene, p);
        if (inventory.empty()) return;
        if (!scene.valid(p.weapon) || !scene.has<Weapon>(p.weapon)) p.weapon = inventory.front();
        for (Entity we : inventory) {
            Weapon& w = scene.get<Weapon>(we);
            w.initialReserve = w.reserveAmmo;
            w.aim = 0.0f;
            w.equipTimer = 0.0f;
            scene.get<EntityInfo>(we).active = we == p.weapon;
        }
        p.previousWeapon = NullEntity;
    });
}

void WeaponSystem::equip(Scene& scene, Player& p, Entity weapon) {
    if (weapon == p.weapon || !scene.valid(weapon) || !scene.has<Weapon>(weapon)) return;
    if (Weapon* old = equippedWeapon(scene, p)) {
        old->reloading = false;
        old->aim = 0.0f;
        scene.get<EntityInfo>(p.weapon).active = false;
    }
    p.previousWeapon = p.weapon;
    p.weapon = weapon;
    p.aimToggled = false;
    p.switchBanner = 1.6f;
    Weapon& w = scene.get<Weapon>(weapon);
    w.equipTimer = w.equipTime;
    w.aim = 0.0f;
    w.queuedShot = 0.0f;
    scene.get<EntityInfo>(weapon).active = true;
}

void WeaponSystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();
    const GameSettings& settings = Settings::get();

    scene.each<Player>([&](Entity playerEntity, Player& p) {
        std::vector<Entity> inventory = weaponInventory(scene, p);
        if (inventory.empty()) return;
        if (!scene.valid(p.weapon) || !scene.has<Weapon>(p.weapon)) p.weapon = inventory.front();

        // --- Switching: number keys, mouse wheel, Q for the previous weapon -----
        if (!p.dead) {
            Entity want = NullEntity;
            for (int i = 0; i < 9; ++i) {
                if (!Input::getKeyDown(Key::D1 + i)) continue;
                for (Entity we : inventory)
                    if (scene.get<Weapon>(we).slot == i + 1) want = we;
            }
            float scroll = Input::scrollDelta();
            if (scroll != 0.0f && inventory.size() > 1) {
                int index = (int)(std::find(inventory.begin(), inventory.end(), p.weapon) - inventory.begin());
                int n = (int)inventory.size();
                index = (index + (scroll < 0.0f ? 1 : n - 1)) % n;
                want = inventory[(size_t)index];
            }
            if (Input::getKeyDown(Key::Q) && scene.valid(p.previousWeapon)) want = p.previousWeapon;
            if (want != NullEntity) equip(scene, p, want);
        }

        Weapon& w = scene.get<Weapon>(p.weapon);
        Transform& wt = scene.get<Transform>(p.weapon);

        w.cooldown -= dt;
        w.flashTimer -= dt;
        w.queuedShot -= dt;
        w.equipTimer = std::max(0.0f, w.equipTimer - dt);
        w.kick = std::max(0.0f, w.kick - dt * 7.0f);
        w.bloom = std::max(0.0f, w.bloom - dt * 4.0f);

        auto startReload = [&]() {
            if (w.reloading || w.ammo >= w.magazineSize || w.reserveAmmo <= 0) return;
            w.reloading = true;
            w.reloadTimer = w.reloadTime;
        };

        if (w.reloading) {
            w.reloadTimer -= dt;
            if (w.reloadTimer <= 0.0f) {
                int take = std::min(w.magazineSize - w.ammo, w.reserveAmmo);
                w.ammo += take;
                w.reserveAmmo -= take;
                w.reloading = false;
            }
        }
        bool ready = !p.dead && !w.reloading && w.equipTimer <= 0.0f;
        if (ready && Input::getKeyDown(Key::R)) startReload();

        // --- Aiming down sights ------------------------------------------------
        bool aimInput;
        if (settings.holdToAim) {
            aimInput = Input::getMouseButton(Mouse::Right);
        } else {
            if (Input::getMouseButtonDown(Mouse::Right)) p.aimToggled = !p.aimToggled;
            aimInput = p.aimToggled;
        }
        bool canAim = ready && !p.sprinting;
        float aimTarget = aimInput && canAim ? 1.0f : 0.0f;
        w.aim = Math::lerp(w.aim, aimTarget, Math::damp(w.aimSpeed, dt));
        if (std::fabs(w.aim - aimTarget) < 0.002f) w.aim = aimTarget;
        float aim = smooth(w.aim);
        bool scoped = w.scope && w.aim > 0.92f;

        // Camera zoom. Scopes snap to their magnification once the overlay is up.
        if (scene.valid(p.camera))
            if (Camera* cam = scene.tryGet<Camera>(p.camera)) {
                float base = settings.fieldOfView;
                cam->fieldOfView = w.scope ? (scoped ? w.aimFov : Math::lerp(base, base * 0.8f, aim)) : Math::lerp(base, w.aimFov, aim);
            }

        // --- Firing --------------------------------------------------------------
        bool trigger;
        if (w.automatic) {
            trigger = Input::getMouseButton(Mouse::Left);
        } else {
            if (Input::getMouseButtonDown(Mouse::Left)) w.queuedShot = 0.2f;
            trigger = w.queuedShot > 0.0f;
        }
        if (ready && trigger && w.cooldown <= 0.0f && scene.valid(p.camera)) {
            w.queuedShot = 0.0f;
            if (w.ammo <= 0) {
                startReload();
            } else {
                w.ammo--;
                w.cooldown = std::max(w.cooldown, 0.0f) + 1.0f / std::max(w.fireRate, 0.1f);
                w.kick = 1.0f;
                w.flashTimer = 0.05f;
                float recoil = w.recoil * Math::lerp(1.0f, 0.6f, aim);
                p.recoil += recoil;
                p.yaw += Random::range(-0.3f, 0.3f) * recoil;

                const Transform& ct = scene.get<Transform>(p.camera);
                glm::vec3 origin = ct.worldPosition();
                glm::vec3 fwd = ct.forward(), right = ct.right(), up = ct.up();
                float spreadDeg = (w.spread + w.bloom) * Math::lerp(1.0f, w.aimSpread, aim);
                w.bloom = std::min(w.bloom + 0.3f, 3.0f);
                glm::vec3 muzzle = origin + fwd * 0.6f;
                if (scene.valid(w.muzzleFlash)) muzzle = scene.get<Transform>(w.muzzleFlash).worldPosition();
                if (scoped) muzzle = origin - up * 0.08f; // tracer starts under the scope

                if (w.kind == WeaponKind::Launcher) {
                    // Aim the rocket from the muzzle at whatever the crosshair points at.
                    glm::vec3 dir = coneDirection(fwd, right, up, spreadDeg);
                    RaycastHit hit;
                    glm::vec3 target = origin + dir * w.range;
                    if (physics.raycast(Ray{origin, dir}, w.range, hit, playerEntity)) target = hit.point;
                    glm::vec3 shot = glm::distance(target, muzzle) > 1.5f ? glm::normalize(target - muzzle) : dir;
                    Prefabs::rocket(scene, muzzle, shot * w.projectileSpeed, w.damage, w.explosionRadius, playerEntity);
                    p.shake = std::min(1.0f, p.shake + 0.25f);
                } else {
                    int pellets = std::max(w.pellets, 1);
                    for (int i = 0; i < pellets; ++i) {
                        glm::vec3 dir = coneDirection(fwd, right, up, spreadDeg);
                        RaycastHit hit;
                        glm::vec3 end = origin + dir * w.range;
                        if (physics.raycast(Ray{origin, dir}, w.range, hit, playerEntity)) {
                            end = hit.point;
                            bool hurt = Combat::applyDamage(scene, hit.entity, w.damage, dir, playerEntity);
                            if (RigidBody* rb = scene.tryGet<RigidBody>(hit.entity)) {
                                if (!rb->isKinematic) rb->velocity += dir * w.impactForce / std::max(rb->mass, 0.1f) + glm::vec3(0, 1.0f, 0);
                            }
                            glm::vec4 sparks = hurt && scene.has<Enemy>(hit.entity) ? glm::vec4(4.0f, 1.2f, 0.4f, 1.0f) : glm::vec4(3.0f, 2.4f, 1.2f, 1.0f);
                            Prefabs::impact(scene, hit.point, hit.normal, sparks);
                        }
                        // One tracer per shot for pellet weapons keeps the screen readable.
                        if (i < 3) DebugDraw::line(muzzle, end, glm::vec4(1.0f, 0.85f, 0.5f, pellets > 1 ? 0.45f : 0.7f), 0.04f);
                    }
                }
                if (w.ammo == 0) w.queuedShot = 0.0f;
            }
        }

        // --- Viewmodel animation: aim blend, sway, bob, recoil, reload and equip dips --
        w.sprint = Math::lerp(w.sprint, p.sprinting ? 1.0f : 0.0f, Math::damp(8.0f, dt));
        float steady = 1.0f - 0.85f * aim;
        glm::vec2 md = Input::mouseDelta();
        glm::vec2 targetSway = glm::clamp(glm::vec2(-md.x, md.y) * 0.0012f, glm::vec2(-0.05f), glm::vec2(0.05f)) * steady;
        w.sway = glm::mix(w.sway, targetSway, Math::damp(12.0f, dt));
        float bobX = std::cos(p.bobPhase) * 0.012f * p.bobAmount * steady;
        float bobY = std::fabs(std::sin(p.bobPhase)) * 0.014f * p.bobAmount * steady;
        float r = 0.0f;
        if (w.reloading) r = std::sin(Math::clamp01(1.0f - w.reloadTimer / std::max(w.reloadTime, 0.01f)) * Math::PI);
        float e = w.equipTime > 0.0f ? smooth(Math::clamp01(w.equipTimer / w.equipTime)) : 0.0f;
        float kick = w.kick * Math::lerp(1.0f, 0.5f, aim);
        glm::vec3 base = glm::mix(w.restPosition, w.aimPosition, aim);
        wt.position = base + glm::vec3(w.sway.x + bobX - w.sprint * 0.05f, w.sway.y - bobY - r * 0.12f - e * 0.3f - w.sprint * 0.04f, kick * 0.07f);
        wt.rotation = glm::vec3(kick * 5.0f - r * 28.0f - e * 35.0f - w.sprint * 12.0f, w.sway.x * 60.0f + w.sprint * 30.0f, r * 18.0f + w.sprint * 10.0f);
        if (p.dead) wt.position.y -= 1.0f;
        if (MeshRenderer* mr = scene.tryGet<MeshRenderer>(p.weapon)) mr->visible = !scoped;

        // Muzzle flash mesh + light
        if (scene.valid(w.muzzleFlash)) {
            bool on = w.flashTimer > 0.0f;
            if (MeshRenderer* mr = scene.tryGet<MeshRenderer>(w.muzzleFlash)) mr->visible = on && !scoped;
            Transform& ft = scene.get<Transform>(w.muzzleFlash);
            if (on) {
                ft.rotation.z = Random::range(0.0f, 360.0f);
                ft.scale = glm::vec3(Random::range(0.8f, 1.2f));
            }
            if (Light* l = scene.tryGet<Light>(w.muzzleFlash)) l->intensity = on ? 4.0f : 0.0f;
        }
    });
}

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------
void EnemySystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();
    GameManager* gm = findGameManager(scene);
    Entity pe = findPlayer(scene);
    bool hasTarget = scene.valid(pe) && !scene.get<Player>(pe).dead;
    glm::vec3 target = hasTarget ? scene.get<Transform>(pe).position + glm::vec3(0, 0.4f, 0) : glm::vec3(0.0f);
    glm::vec3 targetVel = hasTarget && scene.has<CharacterController>(pe) ? scene.get<CharacterController>(pe).velocity : glm::vec3(0.0f);

    scene.each<Enemy, Transform, Health>([&](Entity e, Enemy& en, Transform& t, Health& hp) {
        if (hp.current <= 0.0f) {
            Prefabs::explosion(scene, t.position);
            if (gm) {
                gm->score += en.scoreValue;
                gm->kills++;
            }
            scene.destroyEntity(e);
            return;
        }

        en.hitFlash = std::max(0.0f, en.hitFlash - dt);
        if (MeshRenderer* mr = scene.tryGet<MeshRenderer>(e)) mr->tint = en.hitFlash > 0.0f ? glm::vec4(1.0f, 0.3f, 0.25f, 1.0f) : glm::vec4(1.0f);
        en.phase += dt;

        glm::vec3 pos = t.position;
        glm::vec3 desired(0.0f);
        if (hasTarget) {
            glm::vec3 to = target - pos;
            glm::vec3 flat(to.x, 0.0f, to.z);
            float dist = glm::length(flat);
            flat = dist > 1e-3f ? flat / dist : glm::vec3(0, 0, 1);

            if (dist > en.preferredDistance + 1.5f) desired += flat * en.speed;
            else if (dist < en.preferredDistance - 1.5f) desired -= flat * en.speed * 0.8f;

            en.strafeTimer -= dt;
            if (en.strafeTimer <= 0.0f) {
                if (Random::value() < 0.6f) en.strafeDir = -en.strafeDir;
                en.strafeTimer = Random::range(1.5f, 3.5f);
            }
            glm::vec3 side = glm::normalize(glm::cross(flat, glm::vec3(0, 1, 0)));
            desired += side * en.strafeDir * en.speed * 0.55f;

            float desiredY = std::max(target.y + 1.0f, 2.0f) + std::sin(en.phase * 1.7f) * 0.35f;
            desired.y = (desiredY - pos.y) * 2.0f;

            // Face the player, bank into strafes.
            glm::vec3 face = Math::lookRotation(to);
            face.z = -glm::dot(en.velocity, side) * 3.0f;
            t.rotation = face;

            // Shoot when the player is visible.
            en.fireTimer -= dt;
            float fullDist = glm::length(to);
            if (en.fireTimer <= 0.0f && fullDist < en.attackRange) {
                RaycastHit hit;
                if (physics.raycast(Ray{pos, to / fullDist}, en.attackRange, hit, e) && hit.entity == pe) {
                    glm::vec3 aim = target + targetVel * (fullDist / en.projectileSpeed) * 0.5f + Random::insideUnitSphere() * 0.6f;
                    glm::vec3 dir = glm::normalize(aim - pos);
                    Prefabs::plasmaBolt(scene, pos + dir * 0.9f, dir * en.projectileSpeed, en.damage, e);
                    en.fireTimer = en.fireInterval * Random::range(0.8f, 1.25f);
                } else {
                    en.fireTimer = 0.3f;
                }
            }
        } else {
            desired.y = (2.5f - pos.y) * 1.5f;
        }

        en.velocity = glm::mix(en.velocity, desired, Math::damp(2.5f, dt));
        MoveResult res = physics.moveBox(pos, glm::vec3(0.65f, 0.4f, 0.65f), en.velocity * dt, e);
        for (int axis = 0; axis < 3; ++axis) {
            if (!res.blocked[axis]) continue;
            en.velocity[axis] *= -0.3f;
            if (axis != 1) en.strafeDir = -en.strafeDir;
        }
        t.position = res.position;
    });
}

// ---------------------------------------------------------------------------
// Projectiles: drone plasma bolts and player rockets
// ---------------------------------------------------------------------------
void ProjectileSystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();

    scene.each<Projectile, Transform>([&](Entity e, Projectile& pr, Transform& t) {
        glm::vec3 delta = pr.velocity * dt;
        float len = glm::length(delta);
        if (len < 1e-6f) return;
        glm::vec3 dir = delta / len;
        RaycastHit hit;
        if (physics.raycast(Ray{t.position, dir}, len + 0.1f, hit, pr.owner)) {
            if (pr.explosionRadius > 0.0f) {
                // Pull the blast slightly off the surface so walls don't block its own line-of-sight checks.
                Combat::explode(scene, hit.point + hit.normal * 0.25f, pr.explosionRadius, pr.damage, 16.0f, pr.owner);
            } else {
                // Drones don't hurt each other, but their bolts do set off barrels.
                bool friendly = scene.valid(pr.owner) && scene.has<Enemy>(pr.owner) && scene.has<Enemy>(hit.entity);
                if (!friendly) Combat::applyDamage(scene, hit.entity, pr.damage, dir, pr.owner);
                Prefabs::impact(scene, hit.point, hit.normal, glm::vec4(3.0f, 1.2f, 0.4f, 1.0f));
            }
            scene.destroyEntity(e);
            return;
        }
        t.position += delta;
        // Rockets that fly out of the arena detonate in the air when their lifetime runs out.
        if (pr.explosionRadius > 0.0f)
            if (Lifetime* life = scene.tryGet<Lifetime>(e); life && life->seconds <= dt * 1.5f) {
                Combat::explode(scene, t.position, pr.explosionRadius, pr.damage, 16.0f, pr.owner);
                scene.destroyEntity(e);
            }
    });
}

// ---------------------------------------------------------------------------
// Explosive props
// ---------------------------------------------------------------------------
namespace {
void setBurning(Scene& scene, Explosive& ex, bool burning) {
    ex.burning = burning;
    if (scene.valid(ex.fire)) scene.get<EntityInfo>(ex.fire).active = burning;
}
} // namespace

void ExplosiveSystem::reset(Scene& scene) {
    scene.each<Explosive, Health, EntityInfo>([&](Entity, Explosive& ex, Health& hp, EntityInfo& info) {
        hp.current = hp.max;
        ex.exploded = false;
        ex.fuse = -1.0f;
        setBurning(scene, ex, false);
        info.active = true;
    });
}

void ExplosiveSystem::onUpdate(Scene& scene, float dt) {
    Entity pe = findPlayer(scene);
    glm::vec3 playerPos = scene.valid(pe) ? scene.get<Transform>(pe).position : glm::vec3(1e6f);

    scene.each<Explosive, Health, EntityInfo>([&](Entity e, Explosive& ex, Health& hp, EntityInfo& info) {
        if (ex.exploded) {
            if (ex.respawnTime <= 0.0f) return;
            ex.respawnTimer -= dt;
            // Respawn out of the player's way (never inside them).
            if (ex.respawnTimer <= 0.0f && glm::distance(playerPos, scene.get<Transform>(e).worldPosition()) > 4.0f) {
                hp.current = hp.max;
                ex.exploded = false;
                ex.fuse = -1.0f;
                info.active = true;
                Prefabs::impact(scene, Combat::colliderCenter(scene, e), glm::vec3(0, 1, 0), glm::vec4(0.6f, 1.5f, 3.0f, 1.0f));
            }
            return;
        }

        if (hp.current <= 0.0f) {
            // Shot to death: detonate now. Caught in another blast: wait for the fuse.
            if (ex.fuse > 0.0f) {
                ex.fuse -= dt;
                if (ex.fuse > 0.0f) {
                    if (!ex.burning) setBurning(scene, ex, true);
                    return;
                }
            }
            ex.exploded = true;
            ex.fuse = -1.0f;
            ex.respawnTimer = ex.respawnTime;
            setBurning(scene, ex, false);
            info.active = false;
            Combat::explode(scene, Combat::colliderCenter(scene, e), ex.radius, ex.damage, ex.force, NullEntity, e);
            return;
        }

        if (!ex.burning && hp.current < hp.max * ex.igniteBelow) setBurning(scene, ex, true);
        if (ex.burning) hp.current -= ex.burnRate * dt;
    });
}

// ---------------------------------------------------------------------------
// Pickups
// ---------------------------------------------------------------------------
void PickupSystem::onUpdate(Scene& scene, float dt) {
    Entity pe = findPlayer(scene);
    if (!scene.valid(pe)) return;
    Player& player = scene.get<Player>(pe);
    glm::vec3 playerPos = scene.get<Transform>(pe).position;
    GameManager* gm = findGameManager(scene);

    scene.each<Pickup, Transform, EntityInfo>([&](Entity, Pickup& pk, Transform& t, EntityInfo& info) {
        if (!pk.available) {
            pk.timer -= dt;
            if (pk.timer <= 0.0f) {
                pk.available = true;
                info.active = true;
            }
            return;
        }
        if (player.dead || glm::distance(playerPos, t.worldPosition()) > 1.6f) return;

        bool used = false;
        char toast[64] = "";
        if (pk.type == PickupType::Health) {
            Health* hp = scene.tryGet<Health>(pe);
            if (hp && hp->current < hp->max) {
                hp->current = std::min(hp->max, hp->current + pk.amount);
                std::snprintf(toast, sizeof(toast), "+%d HEALTH", (int)pk.amount);
                used = true;
            }
        } else {
            // Ammo boxes top up every weapon in the inventory.
            for (Entity we : weaponInventory(scene, player)) {
                Weapon& w = scene.get<Weapon>(we);
                int room = w.maxReserve - w.reserveAmmo;
                int give = std::min(room, std::max(1, (int)std::lround(w.ammoPerPickup * pk.amount)));
                if (give <= 0) continue;
                w.reserveAmmo += give;
                used = true;
            }
            if (used) std::snprintf(toast, sizeof(toast), "+AMMO");
        }
        if (used) {
            pk.available = false;
            pk.timer = pk.respawnTime;
            info.active = false;
            if (gm) {
                gm->toast = toast;
                gm->toastTimer = 1.5f;
            }
        }
    });
}

// ---------------------------------------------------------------------------
// Spinner
// ---------------------------------------------------------------------------
void SpinnerSystem::onUpdate(Scene& scene, float dt) {
    float time = (float)Time::time();
    scene.each<Spinner, Transform>([&](Entity, Spinner& s, Transform& t) {
        if (!s.initialized) {
            s.baseY = t.position.y;
            s.initialized = true;
        }
        t.rotation = glm::mod(t.rotation + s.speed * dt + glm::vec3(180.0f), glm::vec3(360.0f)) - glm::vec3(180.0f);
        if (s.bobAmplitude > 0.0f) t.position.y = s.baseY + std::sin(time * s.bobFrequency + s.baseY) * s.bobAmplitude;
    });
}

} // namespace FPS
