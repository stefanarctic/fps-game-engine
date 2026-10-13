// Procedural construction of the default "Arena" level. The result is saved to
// assets/scenes/Arena.scene the first time the editor runs, after which the
// level can be edited (and re-saved) in the editor like any other scene.
#include "FPSGame.h"
#include "GameComponents.h"

namespace FPS {

using namespace Forge;

namespace {

struct Builder {
    Scene& scene;

    Entity block(const std::string& name, const glm::vec3& pos, const glm::vec3& size, const std::string& material,
                 Entity parent = NullEntity, bool collider = true) {
        Entity e = scene.createEntity(name, parent);
        Transform& t = scene.get<Transform>(e);
        t.position = pos;
        t.scale = size;
        MeshRenderer& mr = scene.add<MeshRenderer>(e);
        mr.mesh = "builtin:Cube";
        mr.material = material;
        if (collider) scene.add<BoxCollider>(e);
        return e;
    }

    Entity model(const std::string& name, const std::string& mesh, const glm::vec3& pos, const glm::vec3& rot = glm::vec3(0.0f),
                 Entity parent = NullEntity) {
        Entity e = scene.createEntity(name, parent);
        Transform& t = scene.get<Transform>(e);
        t.position = pos;
        t.rotation = rot;
        scene.add<MeshRenderer>(e).mesh = mesh;
        return e;
    }

    Entity group(const std::string& name) { return scene.createEntity(name); }

    Entity pointLight(const std::string& name, const glm::vec3& pos, const glm::vec3& color, float intensity, float range,
                      Entity parent = NullEntity) {
        Entity e = scene.createEntity(name, parent);
        scene.get<Transform>(e).position = pos;
        Light& l = scene.add<Light>(e);
        l.type = LightType::Point;
        l.color = color;
        l.intensity = intensity;
        l.range = range;
        return e;
    }

    Entity crate(const glm::vec3& pos, float yaw, bool dynamic, Entity parent) {
        Entity e = model("Crate", "models/crate.obj", pos, glm::vec3(0, yaw, 0), parent);
        BoxCollider& box = scene.add<BoxCollider>(e);
        box.center = glm::vec3(0, 0.5f, 0);
        box.size = glm::vec3(1.0f);
        if (dynamic) {
            RigidBody& rb = scene.add<RigidBody>(e);
            rb.mass = 2.0f;
            rb.bounciness = 0.15f;
        }
        return e;
    }

    // Explosive barrel: one rifle hit sets it on fire, two blow it up.
    Entity barrel(const glm::vec3& pos, Entity parent) {
        Entity e = model("Explosive Barrel", "models/barrel.obj", pos, glm::vec3(0, Random::range(0.0f, 360.0f), 0), parent);
        BoxCollider& box = scene.add<BoxCollider>(e);
        box.center = glm::vec3(0, 0.6f, 0);
        box.size = glm::vec3(0.8f, 1.2f, 0.8f);
        Health& hp = scene.add<Health>(e);
        hp.current = hp.max = 40.0f;
        Explosive& ex = scene.add<Explosive>(e);

        // Flames on the lid, hidden until the barrel is burning.
        Entity fire = scene.createEntity("Fire", e);
        scene.get<Transform>(fire).position = glm::vec3(0, 1.22f, 0);
        scene.get<EntityInfo>(fire).active = false;
        ParticleEmitter& flames = scene.add<ParticleEmitter>(fire);
        flames.rate = 45.0f;
        flames.lifetime = 0.55f;
        flames.startSpeed = 1.8f;
        flames.direction = glm::vec3(0, 1, 0);
        flames.spread = 0.22f;
        flames.gravity = -0.05f;
        flames.startSize = 0.38f;
        flames.endSize = 0.05f;
        flames.startColor = glm::vec4(3.5f, 1.4f, 0.35f, 0.9f);
        flames.endColor = glm::vec4(1.0f, 0.2f, 0.05f, 0.0f);
        Light& glow = scene.add<Light>(fire);
        glow.type = LightType::Point;
        glow.color = glm::vec3(1.0f, 0.55f, 0.2f);
        glow.intensity = 2.5f;
        glow.range = 5.0f;
        ex.fire = fire;
        return e;
    }

    // A first-person weapon parented to the camera, with its own muzzle flash.
    Weapon& weapon(Entity camera, const std::string& name, const std::string& mesh, float scale, const glm::vec3& muzzle) {
        Entity e = scene.createEntity(name, camera);
        Weapon& w = scene.add<Weapon>(e);
        scene.get<Transform>(e).scale = glm::vec3(scale);
        MeshRenderer& mr = scene.add<MeshRenderer>(e);
        mr.mesh = mesh;
        mr.layer = RenderLayer::Viewmodel;
        mr.castShadows = false;

        Entity flash = scene.createEntity("Muzzle Flash", e);
        scene.get<Transform>(flash).position = muzzle;
        MeshRenderer& fmr = scene.add<MeshRenderer>(flash);
        fmr.mesh = "models/muzzleflash.obj";
        fmr.material = "materials/MuzzleFlash.mat";
        fmr.layer = RenderLayer::Viewmodel;
        fmr.castShadows = false;
        fmr.visible = false;
        Light& fl = scene.add<Light>(flash);
        fl.type = LightType::Point;
        fl.color = glm::vec3(1.0f, 0.75f, 0.4f);
        fl.intensity = 0.0f;
        fl.range = 7.0f;
        w.muzzleFlash = flash;
        return w;
    }

    Entity pickup(const std::string& name, const glm::vec3& pos, PickupType type, Entity parent) {
        bool health = type == PickupType::Health;
        Entity e = scene.createEntity(name, parent);
        Transform& t = scene.get<Transform>(e);
        t.position = pos;
        t.scale = glm::vec3(0.45f);
        MeshRenderer& mr = scene.add<MeshRenderer>(e);
        mr.mesh = health ? "builtin:Sphere" : "builtin:Cube";
        mr.material = health ? "materials/HealthPickup.mat" : "materials/AmmoPickup.mat";
        mr.castShadows = false;
        Pickup& p = scene.add<Pickup>(e);
        p.type = type;
        p.amount = health ? 35.0f : 1.0f; // ammo: one box = each weapon's Ammo Per Pickup
        Spinner& s = scene.add<Spinner>(e);
        s.speed = glm::vec3(0, 120, 0);
        s.bobAmplitude = 0.15f;
        Light& l = scene.add<Light>(e);
        l.type = LightType::Point;
        l.color = health ? glm::vec3(0.3f, 1.0f, 0.4f) : glm::vec3(1.0f, 0.8f, 0.3f);
        l.intensity = 1.5f;
        l.range = 4.0f;
        return e;
    }
};

} // namespace

void buildArena(Scene& scene) {
    Random::seed(42);
    Builder b{scene};
    scene.name = "Arena";

    Environment& env = scene.environment;
    env = Environment();
    env.skyColor = glm::vec3(0.34f, 0.52f, 0.82f);
    env.horizonColor = glm::vec3(0.80f, 0.78f, 0.74f);
    env.groundColor = glm::vec3(0.30f, 0.27f, 0.25f);
    env.ambientIntensity = 0.55f;
    env.fogColor = glm::vec3(0.70f, 0.72f, 0.75f);
    env.fogDensity = 0.010f;

    // --- Lighting ---------------------------------------------------------
    Entity sun = scene.createEntity("Directional Light");
    scene.get<Transform>(sun).position = glm::vec3(0, 12, 0);
    scene.get<Transform>(sun).rotation = glm::vec3(-48, 35, 0);
    Light& sl = scene.add<Light>(sun);
    sl.type = LightType::Directional;
    sl.color = glm::vec3(1.0f, 0.94f, 0.82f);
    sl.intensity = 1.25f;

    // --- Geometry -----------------------------------------------------------
    Entity level = b.group("Level");
    b.block("Floor", glm::vec3(0, -0.5f, 0), glm::vec3(60, 1, 60), "materials/Floor.mat", level);
    Entity walls = scene.createEntity("Walls", level);
    b.block("Wall North", glm::vec3(0, 3, -30.5f), glm::vec3(62, 6, 1), "materials/Wall.mat", walls);
    b.block("Wall South", glm::vec3(0, 3, 30.5f), glm::vec3(62, 6, 1), "materials/Wall.mat", walls);
    b.block("Wall East", glm::vec3(30.5f, 3, 0), glm::vec3(1, 6, 60), "materials/Wall.mat", walls);
    b.block("Wall West", glm::vec3(-30.5f, 3, 0), glm::vec3(1, 6, 60), "materials/Wall.mat", walls);
    // Glowing base strips along the walls (decoration, no colliders)
    b.block("Strip North", glm::vec3(0, 0.25f, -29.97f), glm::vec3(60, 0.12f, 0.06f), "materials/GlowCyan.mat", walls, false);
    b.block("Strip South", glm::vec3(0, 0.25f, 29.97f), glm::vec3(60, 0.12f, 0.06f), "materials/GlowCyan.mat", walls, false);
    b.block("Strip East", glm::vec3(29.97f, 0.25f, 0), glm::vec3(0.06f, 0.12f, 60), "materials/GlowCyan.mat", walls, false);
    b.block("Strip West", glm::vec3(-29.97f, 0.25f, 0), glm::vec3(0.06f, 0.12f, 60), "materials/GlowCyan.mat", walls, false);

    // Central platform with a monolith
    Entity center = scene.createEntity("Center", level);
    b.block("Platform", glm::vec3(0, 0.5f, 0), glm::vec3(9, 1, 9), "materials/Metal.mat", center);
    b.block("Platform Trim", glm::vec3(0, 1.02f, 0), glm::vec3(9.1f, 0.06f, 9.1f), "materials/Hazard.mat", center, false);
    b.block("Monolith", glm::vec3(0, 3.5f, 0), glm::vec3(1.6f, 5, 1.6f), "materials/Dark.mat", center);
    b.block("Monolith Glow", glm::vec3(0, 3.5f, 0), glm::vec3(1.65f, 0.15f, 1.65f), "materials/GlowOrange.mat", center, false);
    b.block("Monolith Cap", glm::vec3(0, 6.05f, 0), glm::vec3(2.0f, 0.1f, 2.0f), "materials/Trim.mat", center);
    b.pointLight("Monolith Light", glm::vec3(0, 3.5f, 2.2f), glm::vec3(1.0f, 0.55f, 0.2f), 2.0f, 9.0f, center);

    // Corner bunkers (L-shaped low walls)
    Entity bunkers = scene.createEntity("Bunkers", level);
    const glm::vec2 corners[4] = {{-18, -18}, {18, -18}, {-18, 18}, {18, 18}};
    for (int i = 0; i < 4; ++i) {
        glm::vec2 c = corners[i];
        float sx = c.x > 0 ? -1.0f : 1.0f, sz = c.y > 0 ? -1.0f : 1.0f;
        Entity g = scene.createEntity("Bunker " + std::to_string(i + 1), bunkers);
        b.block("Wall A", glm::vec3(c.x, 0.7f, c.y), glm::vec3(5, 1.4f, 0.6f), "materials/Wall.mat", g);
        b.block("Wall B", glm::vec3(c.x + sx * 2.2f, 0.7f, c.y + sz * 2.0f), glm::vec3(0.6f, 1.4f, 4.6f), "materials/Wall.mat", g);
        b.block("Cap A", glm::vec3(c.x, 1.43f, c.y), glm::vec3(5.05f, 0.06f, 0.65f), "materials/Trim.mat", g, false);
        b.block("Cap B", glm::vec3(c.x + sx * 2.2f, 1.43f, c.y + sz * 2.0f), glm::vec3(0.65f, 0.06f, 4.65f), "materials/Trim.mat", g, false);
    }

    // Pillars with lamps
    Entity pillars = scene.createEntity("Pillars", level);
    const glm::vec2 pillarPos[4] = {{-11, -11}, {11, -11}, {-11, 11}, {11, 11}};
    for (int i = 0; i < 4; ++i) {
        glm::vec3 p(pillarPos[i].x, 0, pillarPos[i].y);
        Entity pillar = b.model("Pillar", "models/pillar.obj", p, glm::vec3(0), pillars);
        BoxCollider& box = scene.add<BoxCollider>(pillar);
        box.center = glm::vec3(0, 2.25f, 0);
        box.size = glm::vec3(1.2f, 4.5f, 1.2f);
        bool cyan = (i % 2) == 0;
        b.block("Lamp", glm::vec3(0, 4.75f, 0), glm::vec3(0.5f, 0.5f, 0.5f), cyan ? "materials/GlowCyan.mat" : "materials/GlowOrange.mat", pillar, false);
        b.pointLight("Lamp Light", glm::vec3(0, 4.7f, 0), cyan ? glm::vec3(0.4f, 0.8f, 1.0f) : glm::vec3(1.0f, 0.6f, 0.3f), 2.2f, 13.0f, pillar);
    }

    // Low cover walls
    Entity cover = scene.createEntity("Cover", level);
    b.block("Cover", glm::vec3(-6, 0.6f, 12), glm::vec3(4, 1.2f, 0.5f), "materials/Trim.mat", cover);
    b.block("Cover", glm::vec3(7, 0.6f, 14), glm::vec3(4, 1.2f, 0.5f), "materials/Trim.mat", cover);
    b.block("Cover", glm::vec3(-8, 0.6f, -13), glm::vec3(4, 1.2f, 0.5f), "materials/Trim.mat", cover);
    b.block("Cover", glm::vec3(6, 0.6f, -12), glm::vec3(0.5f, 1.2f, 4), "materials/Trim.mat", cover);
    b.block("Cover", glm::vec3(-22, 0.6f, 0), glm::vec3(0.5f, 1.2f, 5), "materials/Trim.mat", cover);
    b.block("Cover", glm::vec3(22, 0.6f, 2), glm::vec3(0.5f, 1.2f, 5), "materials/Trim.mat", cover);
    // Jumpable steps up the side of the central platform
    b.block("Step", glm::vec3(0, 0.25f, 5.6f), glm::vec3(3, 0.5f, 2.0f), "materials/Metal.mat", cover);

    // Props: crates (some physics-enabled) and barrels
    Entity props = scene.createEntity("Props", level);
    b.crate(glm::vec3(-14, 0, 4), 10, false, props);
    b.crate(glm::vec3(-14, 1, 4), 35, true, props);
    b.crate(glm::vec3(-12.8f, 0, 4.4f), -8, false, props);
    b.crate(glm::vec3(14, 0, -4), 0, false, props);
    b.crate(glm::vec3(15.1f, 0, -4.2f), 20, true, props);
    b.crate(glm::vec3(14.5f, 1, -4.1f), 45, true, props);
    b.crate(glm::vec3(4, 0, 20), 15, true, props);
    b.crate(glm::vec3(-3, 0, -22), -12, false, props);
    b.crate(glm::vec3(-3, 1, -22), 5, true, props);
    b.crate(glm::vec3(24, 0, 22), 30, true, props);
    b.barrel(glm::vec3(-16, 0, 6), props);
    b.barrel(glm::vec3(-15.2f, 0, 6.8f), props);
    b.barrel(glm::vec3(16, 0, -6.5f), props);
    b.barrel(glm::vec3(9, 0, 20), props);
    b.barrel(glm::vec3(-25, 0, -25), props);
    b.barrel(glm::vec3(25, 0, -25), props);
    b.barrel(glm::vec3(-26, 0, 20), props);
    // A tight cluster in the open for chain reactions
    b.barrel(glm::vec3(8.5f, 0, 5.5f), props);
    b.barrel(glm::vec3(9.4f, 0, 6.1f), props);
    b.barrel(glm::vec3(8.7f, 0, 6.6f), props);
    b.barrel(glm::vec3(-8.0f, 0, -6.0f), props);
    b.barrel(glm::vec3(-8.9f, 0, -6.4f), props);

    // Weapon display: the glTF rifle spinning on a pedestal next to the spawn
    Entity display = scene.createEntity("Weapon Display", level);
    scene.get<Transform>(display).position = glm::vec3(-4, 0, 25);
    b.block("Pedestal", glm::vec3(0, 0.5f, 0), glm::vec3(1.0f, 1.0f, 1.0f), "materials/Dark.mat", display);
    Entity showcase = b.model("Rifle Showcase", "models/rifle.gltf", glm::vec3(0, 1.45f, 0), glm::vec3(0, 0, 0), display);
    scene.get<Transform>(showcase).scale = glm::vec3(1.2f);
    Spinner& spin = scene.add<Spinner>(showcase);
    spin.speed = glm::vec3(0, 45, 0);
    spin.bobAmplitude = 0.05f;
    b.pointLight("Display Light", glm::vec3(0, 2.6f, 0.6f), glm::vec3(1.0f, 0.95f, 0.85f), 1.5f, 5.0f, display);
    Entity droneStatue = b.model("Drone Showcase", "models/drone.glb", glm::vec3(2.5f, 1.6f, 0), glm::vec3(0, 200, 0), display);
    Spinner& ds = scene.add<Spinner>(droneStatue);
    ds.speed = glm::vec3(0, -30, 0);
    ds.bobAmplitude = 0.12f;

    // --- Gameplay -----------------------------------------------------------
    Entity spawns = scene.createEntity("Spawn Points");
    const glm::vec3 spawnPos[6] = {{-24, 3, -24}, {24, 3, -24}, {0, 3.5f, -26}, {-26, 3, -2}, {26, 3, -2}, {0, 8, 0}};
    for (int i = 0; i < 6; ++i) {
        Entity sp = scene.createEntity("Spawn " + std::to_string(i + 1), spawns);
        scene.get<Transform>(sp).position = spawnPos[i];
        scene.add<SpawnPoint>(sp);
    }

    Entity pickups = scene.createEntity("Pickups");
    b.pickup("Health Pack", glm::vec3(-20, 0.8f, 10), PickupType::Health, pickups);
    b.pickup("Health Pack", glm::vec3(20, 0.8f, -10), PickupType::Health, pickups);
    b.pickup("Ammo Box", glm::vec3(20, 0.8f, 10), PickupType::Ammo, pickups);
    b.pickup("Ammo Box", glm::vec3(-20, 0.8f, -10), PickupType::Ammo, pickups);
    b.pickup("Health Pack", glm::vec3(0, 1.8f, 0.0f) + glm::vec3(0, 0, 3.4f), PickupType::Health, pickups);

    Entity manager = scene.createEntity("Game Manager");
    scene.add<GameManager>(manager);

    // Player: body (controller + hitbox) -> camera (pitch) -> weapon viewmodels -> muzzle flashes
    Entity player = scene.createEntity("Player");
    scene.get<EntityInfo>(player).tag = "Player";
    scene.get<Transform>(player).position = glm::vec3(0, 0.95f, 22);
    scene.add<CharacterController>(player);
    BoxCollider& hitbox = scene.add<BoxCollider>(player);
    hitbox.size = glm::vec3(0.7f, 1.8f, 0.7f);
    Health& hp = scene.add<Health>(player);
    hp.current = hp.max = 100.0f;
    Player& pc = scene.add<Player>(player);

    Entity camera = scene.createEntity("Main Camera", player);
    scene.get<EntityInfo>(camera).tag = "MainCamera";
    scene.get<Transform>(camera).position = glm::vec3(0, pc.eyeHeight, 0);
    Camera& cam = scene.add<Camera>(camera);
    cam.fieldOfView = 70.0f;
    cam.nearClip = 0.03f;
    cam.farClip = 400.0f;

    // Aim positions put each model's sight line (see tools/generate_assets.py) on the camera axis:
    // y = -(sight height * scale).
    // 1: assault rifle (glTF) - automatic, red dot sight
    Weapon& rifle = b.weapon(camera, "Assault Rifle", "models/rifle.gltf", 0.62f, glm::vec3(0, 0.045f, -0.8f));
    rifle.slot = 1;
    rifle.aimFov = 50.0f;
    rifle.aimPosition = glm::vec3(0.0f, -0.112f * 0.62f, -0.33f);

    // 2: pump shotgun - 9 pellets, devastating up close
    Weapon& shotgun = b.weapon(camera, "Shotgun", "models/shotgun.glb", 0.62f, glm::vec3(0, 0.025f, -0.7f));
    shotgun.slot = 2;
    shotgun.automatic = false;
    shotgun.damage = 12.0f;
    shotgun.pellets = 9;
    shotgun.fireRate = 1.25f;
    shotgun.range = 45.0f;
    shotgun.spread = 5.0f;
    shotgun.recoil = 4.5f;
    shotgun.magazineSize = shotgun.ammo = 8;
    shotgun.reserveAmmo = 32;
    shotgun.maxReserve = 64;
    shotgun.ammoPerPickup = 16;
    shotgun.reloadTime = 2.0f;
    shotgun.impactForce = 3.0f;
    shotgun.aimFov = 60.0f;
    shotgun.aimSpread = 0.65f;
    shotgun.aimPosition = glm::vec3(0.0f, -0.0505f * 0.62f, -0.25f);
    shotgun.restPosition = glm::vec3(0.17f, -0.16f, -0.48f);

    // 3: sniper rifle - one-shots drones, 6x scope
    Weapon& sniper = b.weapon(camera, "Sniper Rifle", "models/sniper.glb", 0.6f, glm::vec3(0, 0.015f, -0.88f));
    sniper.slot = 3;
    sniper.automatic = false;
    sniper.damage = 150.0f;
    sniper.fireRate = 0.75f;
    sniper.range = 400.0f;
    sniper.spread = 4.0f;  // inaccurate from the hip...
    sniper.aimSpread = 0.0f; // ...perfect through the scope
    sniper.recoil = 5.0f;
    sniper.magazineSize = sniper.ammo = 5;
    sniper.reserveAmmo = 20;
    sniper.maxReserve = 40;
    sniper.ammoPerPickup = 10;
    sniper.reloadTime = 2.4f;
    sniper.impactForce = 20.0f;
    sniper.scope = true;
    sniper.aimFov = 14.0f;
    sniper.aimSpeed = 8.0f;
    sniper.aimPosition = glm::vec3(0.0f, -0.085f * 0.6f, -0.22f);
    sniper.restPosition = glm::vec3(0.15f, -0.15f, -0.48f);

    // 4: rocket launcher - explosive splash damage (and rocket jumps)
    Weapon& launcher = b.weapon(camera, "Rocket Launcher", "models/launcher.glb", 0.6f, glm::vec3(0, 0, -0.7f));
    launcher.slot = 4;
    launcher.kind = WeaponKind::Launcher;
    launcher.automatic = false;
    launcher.damage = 130.0f;
    launcher.fireRate = 0.8f;
    launcher.spread = 0.3f;
    launcher.recoil = 3.0f;
    launcher.magazineSize = launcher.ammo = 3;
    launcher.reserveAmmo = 9;
    launcher.maxReserve = 18;
    launcher.ammoPerPickup = 4;
    launcher.reloadTime = 2.6f;
    launcher.equipTime = 0.5f;
    launcher.projectileSpeed = 30.0f;
    launcher.explosionRadius = 5.5f;
    launcher.aimFov = 58.0f;
    launcher.aimSpread = 0.5f;
    // The sight sits up-left of the tube; when aiming the tube rests on the shoulder beside the head.
    launcher.aimPosition = glm::vec3(0.11f * 0.6f, -0.10f * 0.6f, -0.1f);
    launcher.restPosition = glm::vec3(0.2f, -0.18f, -0.42f);

    // Only the equipped weapon is active; WeaponSystem toggles them when switching.
    Entity rifleEntity = NullEntity;
    for (Entity we : scene.get<Transform>(camera).children) {
        Weapon& w = scene.get<Weapon>(we);
        scene.get<Transform>(we).position = w.restPosition;
        scene.get<EntityInfo>(we).active = w.slot == 1;
        if (w.slot == 1) rifleEntity = we;
    }

    pc.camera = camera;
    pc.weapon = rifleEntity;

    scene.updateTransforms();
    LOG_INFO("Arena built: %zu entities", scene.entityCount());
}

} // namespace FPS
