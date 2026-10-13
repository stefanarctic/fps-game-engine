#include "GameSystems.h"
#include "Prefabs.h"

#include <cmath>

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
        t.position = p.spawnPosition;
        if (Health* hp = scene.tryGet<Health>(pe)) hp->current = hp->max;
        if (CharacterController* cc = scene.tryGet<CharacterController>(pe)) cc->velocity = glm::vec3(0.0f);
        if (scene.valid(p.camera)) {
            scene.get<Transform>(p.camera).position = glm::vec3(0.0f, p.eyeHeight, 0.0f);
            scene.get<Transform>(p.camera).rotation = glm::vec3(0.0f);
        }
        if (scene.valid(p.weapon)) {
            if (Weapon* w = scene.tryGet<Weapon>(p.weapon)) {
                w->ammo = w->magazineSize;
                w->reserveAmmo = std::max(w->reserveAmmo, w->magazineSize * 4);
                w->reloading = false;
            }
        }
    }
    scene.each<Pickup, EntityInfo>([](Entity, Pickup& pk, EntityInfo& info) {
        pk.available = true;
        info.active = true;
    });
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
    scene.each<Player, CharacterController, Transform>([&](Entity e, Player& p, CharacterController& cc, Transform& t) {
        Health* hp = scene.tryGet<Health>(e);
        if (hp && hp->current <= 0.0f) p.dead = true;
        p.damageFlash = std::max(0.0f, p.damageFlash - dt * 1.5f);

        // Mouse look
        if (!p.dead) {
            glm::vec2 md = Input::mouseDelta();
            p.yaw -= md.x * p.mouseSensitivity;
            p.pitch = glm::clamp(p.pitch - md.y * p.mouseSensitivity, -88.0f, 88.0f);
        }
        p.recoil = Math::lerp(p.recoil, 0.0f, Math::damp(7.0f, dt));
        t.rotation = glm::vec3(0.0f, p.yaw, 0.0f);

        // Walk / sprint
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
        bool sprint = Input::getKey(Key::LeftShift) && glm::dot(wish, forward) > 0.5f;
        glm::vec3 target = wish * p.moveSpeed * (sprint ? p.sprintMultiplier : 1.0f);
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

        // Head bob
        float horizontal = glm::length(glm::vec2(cc.velocity.x, cc.velocity.z));
        float targetBob = (cc.grounded && horizontal > 0.5f) ? std::min(horizontal / p.moveSpeed, 1.6f) : 0.0f;
        p.bobAmount = Math::lerp(p.bobAmount, targetBob, Math::damp(10.0f, dt));
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
                ct.rotation = glm::vec3(p.pitch + p.recoil, 0.0f, 0.0f);
            }
        }
    });
}

// ---------------------------------------------------------------------------
// Weapon
// ---------------------------------------------------------------------------
void WeaponSystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();
    GameManager* gm = findGameManager(scene);

    scene.each<Player>([&](Entity playerEntity, Player& p) {
        if (!scene.valid(p.weapon) || !scene.has<Weapon>(p.weapon)) return;
        Weapon& w = scene.get<Weapon>(p.weapon);
        Transform& wt = scene.get<Transform>(p.weapon);

        w.cooldown -= dt;
        w.flashTimer -= dt;
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

        if (!p.dead && !w.reloading && Input::getKeyDown(Key::R)) startReload();

        if (!p.dead && !w.reloading && Input::getMouseButton(Mouse::Left) && w.cooldown <= 0.0f) {
            if (w.ammo <= 0) {
                startReload();
            } else if (scene.valid(p.camera)) {
                // Fire one round
                w.ammo--;
                w.cooldown = std::max(w.cooldown, 0.0f) + 1.0f / std::max(w.fireRate, 0.1f);
                w.kick = 1.0f;
                w.flashTimer = 0.05f;
                p.recoil += w.recoil;
                p.yaw += Random::range(-0.3f, 0.3f) * w.recoil;

                const Transform& ct = scene.get<Transform>(p.camera);
                glm::vec3 origin = ct.worldPosition();
                glm::vec3 fwd = ct.forward(), right = ct.right(), up = ct.up();
                float spreadDeg = w.spread + w.bloom;
                w.bloom = std::min(w.bloom + 0.3f, 3.0f);
                float a = glm::radians(spreadDeg) * std::sqrt(Random::value());
                float theta = Random::range(0.0f, 2.0f * Math::PI);
                glm::vec3 dir = glm::normalize(fwd + (right * std::cos(theta) + up * std::sin(theta)) * std::tan(a));

                glm::vec3 muzzle = origin + fwd * 0.6f;
                if (scene.valid(w.muzzleFlash)) muzzle = scene.get<Transform>(w.muzzleFlash).worldPosition();

                RaycastHit hit;
                glm::vec3 end = origin + dir * w.range;
                if (physics.raycast(Ray{origin, dir}, w.range, hit, playerEntity)) {
                    end = hit.point;
                    glm::vec4 sparks(3.0f, 2.4f, 1.2f, 1.0f);
                    if (Enemy* enemy = scene.tryGet<Enemy>(hit.entity)) {
                        if (Health* hp = scene.tryGet<Health>(hit.entity)) hp->current -= w.damage;
                        enemy->hitFlash = 0.12f;
                        enemy->velocity += dir * 3.0f;
                        if (gm) gm->hitMarker = 0.15f;
                        sparks = glm::vec4(4.0f, 1.2f, 0.4f, 1.0f);
                    }
                    if (RigidBody* rb = scene.tryGet<RigidBody>(hit.entity)) {
                        if (!rb->isKinematic) rb->velocity += dir * w.impactForce / std::max(rb->mass, 0.1f) + glm::vec3(0, 1.0f, 0);
                    }
                    Prefabs::impact(scene, hit.point, hit.normal, sparks);
                }
                DebugDraw::line(muzzle, end, glm::vec4(1.0f, 0.85f, 0.5f, 0.7f), 0.04f);
            }
        }

        // Viewmodel animation: sway, bob, recoil kick, reload dip.
        glm::vec2 md = Input::mouseDelta();
        glm::vec2 targetSway = glm::clamp(glm::vec2(-md.x, md.y) * 0.0012f, glm::vec2(-0.05f), glm::vec2(0.05f));
        w.sway = glm::mix(w.sway, targetSway, Math::damp(12.0f, dt));
        float bobX = std::cos(p.bobPhase) * 0.012f * p.bobAmount;
        float bobY = std::fabs(std::sin(p.bobPhase)) * 0.014f * p.bobAmount;
        float r = 0.0f;
        if (w.reloading) r = std::sin(Math::clamp01(1.0f - w.reloadTimer / w.reloadTime) * Math::PI);
        wt.position = w.restPosition + glm::vec3(w.sway.x + bobX, w.sway.y - bobY - r * 0.12f, w.kick * 0.07f);
        wt.rotation = glm::vec3(w.kick * 5.0f - r * 28.0f, w.sway.x * 60.0f, r * 18.0f);
        if (p.dead) wt.position.y -= 1.0f;

        // Muzzle flash mesh + light
        if (scene.valid(w.muzzleFlash)) {
            bool on = w.flashTimer > 0.0f;
            if (MeshRenderer* mr = scene.tryGet<MeshRenderer>(w.muzzleFlash)) mr->visible = on;
            Transform& ft = scene.get<Transform>(w.muzzleFlash);
            if (on) {
                ft.rotation.z = Random::range(0.0f, 360.0f);
                float s = Random::range(0.8f, 1.2f);
                ft.scale = glm::vec3(s);
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
// Projectiles
// ---------------------------------------------------------------------------
void ProjectileSystem::onUpdate(Scene& scene, float dt) {
    PhysicsWorld& physics = scene.physics();
    Entity pe = findPlayer(scene);

    scene.each<Projectile, Transform>([&](Entity e, Projectile& pr, Transform& t) {
        glm::vec3 delta = pr.velocity * dt;
        float len = glm::length(delta);
        if (len < 1e-6f) return;
        RaycastHit hit;
        if (physics.raycast(Ray{t.position, delta / len}, len + 0.1f, hit, pr.owner)) {
            if (hit.entity == pe) {
                Player& p = scene.get<Player>(pe);
                if (Health* hp = scene.tryGet<Health>(pe); hp && !p.dead) {
                    hp->current = std::max(0.0f, hp->current - pr.damage);
                    p.damageFlash = 1.0f;
                }
            }
            Prefabs::impact(scene, hit.point, hit.normal, glm::vec4(3.0f, 1.2f, 0.4f, 1.0f));
            scene.destroyEntity(e);
            return;
        }
        t.position += delta;
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
        } else if (scene.valid(player.weapon)) {
            if (Weapon* w = scene.tryGet<Weapon>(player.weapon)) {
                w->reserveAmmo += (int)pk.amount;
                std::snprintf(toast, sizeof(toast), "+%d AMMO", (int)pk.amount);
                used = true;
            }
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
