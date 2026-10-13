// Forge test suite: run `ForgeTests.exe` (or `ctest`) from the build directory.
// Covers the ECS, JSON, reflection + scene serialization, math, physics and the
// model loaders (the loaders need an OpenGL context, created hidden).
#include <Forge/Forge.h>

#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace Forge;

namespace {
int g_Passed = 0, g_Failed = 0;
const char* g_Test = "";

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        if (cond) {                                                                          \
            g_Passed++;                                                                      \
        } else {                                                                             \
            g_Failed++;                                                                      \
            std::printf("  FAILED [%s] %s:%d: %s\n", g_Test, __FILE__, __LINE__, #cond);     \
        }                                                                                    \
    } while (0)

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
bool near(const glm::vec3& a, const glm::vec3& b, float eps = 1e-3f) { return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps); }

struct Tagged {
    int value = 0;
    Entity target = NullEntity;
    std::string label = "x";
};

// ---------------------------------------------------------------------------
void testRegistry() {
    g_Test = "Registry";
    Registry r;
    Entity a = r.create(), b = r.create(), c = r.create();
    CHECK(r.valid(a) && r.valid(b) && r.valid(c));
    r.emplace<Tagged>(a).value = 1;
    r.emplace<Tagged>(b).value = 2;
    r.emplace<Tagged>(c).value = 3;
    r.emplace<Lifetime>(b);

    // Reference stability: adding many more components must not move existing ones.
    Tagged* pa = &r.get<Tagged>(a);
    std::vector<Entity> many;
    for (int i = 0; i < 2000; ++i) {
        Entity e = r.create();
        r.emplace<Tagged>(e).value = 100 + i;
        many.push_back(e);
    }
    CHECK(pa == &r.get<Tagged>(a));
    CHECK(pa->value == 1);

    // Swap-and-pop removal keeps other components intact.
    r.remove<Tagged>(a);
    CHECK(!r.has<Tagged>(a));
    CHECK(r.get<Tagged>(b).value == 2 && r.get<Tagged>(c).value == 3);

    // Versioning: destroyed handles become invalid even if the slot is reused.
    r.destroy(b);
    CHECK(!r.valid(b));
    Entity reused = r.create();
    CHECK(ecs::indexOf(reused) == ecs::indexOf(b));
    CHECK(reused != b && !r.valid(b) && r.valid(reused));
    CHECK(!r.has<Lifetime>(reused));

    int count = 0;
    r.each<Tagged>([&](Entity, Tagged&) { count++; });
    CHECK(count == 1 + 2000); // c + many

    // Destroying while iterating is safe (snapshot iteration).
    r.each<Tagged>([&](Entity e, Tagged& t) {
        if (t.value % 2 == 0) r.destroy(e);
    });
    int remaining = 0;
    r.each<Tagged>([&](Entity, Tagged& t) {
        remaining++;
        CHECK(t.value % 2 == 1);
    });
    CHECK(remaining == 1 + 1000);
    CHECK((r.view<Tagged, Lifetime>().empty()));
}

void testJson() {
    g_Test = "Json";
    const char* text = R"({
        // comments are tolerated
        "name": "Arena", "count": 3, "pi": 3.25, "ok": true, "none": null,
        "list": [1, 2, {"deep": "é\n"}], "empty": {}, "arr": []
    })";
    Json j;
    std::string error;
    CHECK(Json::parse(text, j, &error));
    CHECK(j["name"].asString() == "Arena");
    CHECK(j["count"].asInt() == 3);
    CHECK(near(j["pi"].asFloat(), 3.25f));
    CHECK(j["ok"].asBool());
    CHECK(j["none"].isNull());
    CHECK(j["list"].size() == 3);
    CHECK(j["list"][2]["deep"].asString() == "\xC3\xA9\n");
    CHECK(j["missing"]["also"].isNull());

    Json round;
    CHECK(Json::parse(j.dump(2), round));
    CHECK(round.dump(0) == j.dump(0));

    Json bad;
    CHECK(!Json::parse("{\"a\": [1, 2,}", bad, &error));
    CHECK(!error.empty());
}

void testMath() {
    g_Test = "Math";
    glm::vec3 p(1, 2, 3), e(20, -35, 10), s(2, 1, 0.5f);
    glm::vec3 p2, e2, s2;
    CHECK(Math::decompose(Math::compose(p, e, s), p2, e2, s2));
    CHECK(near(p, p2) && near(s, s2) && near(e, e2, 0.01f));

    glm::vec3 dir = Math::directionFromYawPitch(30.0f, -15.0f);
    glm::vec3 rot = Math::lookRotation(dir);
    CHECK(near(rot.y, 30.0f, 0.01f) && near(rot.x, -15.0f, 0.01f));
    glm::vec3 fwd = glm::vec3(Math::eulerToMatrix(rot) * glm::vec4(0, 0, -1, 0));
    CHECK(near(fwd, dir));

    float t;
    glm::vec3 n;
    CHECK(Math::rayAABB(Ray{{0, 5, 0}, {0, -1, 0}}, AABB({-1, -1, -1}, {1, 1, 1}), t, 100.0f, &n));
    CHECK(near(t, 4.0f) && near(n, glm::vec3(0, 1, 0)));
    CHECK(!Math::rayAABB(Ray{{5, 5, 0}, {0, -1, 0}}, AABB({-1, -1, -1}, {1, 1, 1}), t));

    AABB box({-1, -1, -1}, {1, 1, 1});
    AABB rotated = box.transformed(Math::compose({0, 0, 0}, {0, 45, 0}, {1, 1, 1}));
    CHECK(near(rotated.max.x, std::sqrt(2.0f)));

    Frustum f;
    f.fromMatrix(glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f));
    CHECK(f.intersects(AABB({-1, -1, -11}, {1, 1, -9})));
    CHECK(!f.intersects(AABB({-1, -1, 9}, {1, 1, 11})));
}

void testSceneSerialization() {
    g_Test = "Scene";
    registerCoreComponents();
    ComponentRegistry::instance().add<Tagged>("Tagged", "Test").field("Value", &Tagged::value).entity("Target", &Tagged::target).field("Label", &Tagged::label);

    Scene scene("Test");
    Entity parent = scene.createEntity("Parent");
    Entity child = scene.createEntity("Child", parent);
    scene.get<Transform>(parent).position = glm::vec3(5, 0, 0);
    scene.get<Transform>(child).position = glm::vec3(0, 2, 0);
    Light& l = scene.add<Light>(child);
    l.type = LightType::Spot;
    l.intensity = 3.5f;
    Tagged& tg = scene.add<Tagged>(parent);
    tg.value = 42;
    tg.target = child;
    tg.label = "hello";
    scene.environment.fogDensity = 0.05f;
    scene.updateTransforms();
    CHECK(near(scene.get<Transform>(child).worldPosition(), glm::vec3(5, 2, 0)));

    // Round trip
    Json json = SceneSerializer::serialize(scene);
    Scene loaded("Loaded");
    CHECK(SceneSerializer::deserialize(loaded, json));
    CHECK(loaded.entityCount() == 2);
    Entity p2 = loaded.findByName("Parent");
    Entity c2 = loaded.findByName("Child");
    CHECK(loaded.valid(p2) && loaded.valid(c2));
    CHECK(loaded.parentOf(c2) == p2);
    CHECK(loaded.get<EntityInfo>(p2).uuid == scene.get<EntityInfo>(parent).uuid);
    CHECK(loaded.has<Light>(c2) && loaded.get<Light>(c2).type == LightType::Spot && near(loaded.get<Light>(c2).intensity, 3.5f));
    CHECK(loaded.has<Tagged>(p2) && loaded.get<Tagged>(p2).value == 42 && loaded.get<Tagged>(p2).label == "hello");
    CHECK(loaded.get<Tagged>(p2).target == c2); // entity reference resolved through UUIDs
    CHECK(near(loaded.environment.fogDensity, 0.05f));
    loaded.updateTransforms();
    CHECK(near(loaded.get<Transform>(c2).worldPosition(), glm::vec3(5, 2, 0)));

    // Duplicate: fresh UUIDs, internal references remapped to the copies.
    Entity copy = scene.duplicate(parent);
    CHECK(scene.valid(copy) && copy != parent);
    CHECK(scene.get<EntityInfo>(copy).uuid != scene.get<EntityInfo>(parent).uuid);
    CHECK(scene.get<Transform>(copy).children.size() == 1);
    Entity copyChild = scene.get<Transform>(copy).children[0];
    CHECK(scene.get<Tagged>(copy).target == copyChild);

    // Reparenting keeps the world transform.
    scene.setParent(child, NullEntity, true);
    scene.updateTransforms();
    CHECK(near(scene.get<Transform>(child).position, glm::vec3(5, 2, 0)));
    CHECK(!scene.isAncestor(parent, child));

    // Recursive destroy
    size_t before = scene.entityCount();
    scene.destroyEntity(copy);
    CHECK(scene.entityCount() == before - 2);
}

void testPhysics() {
    g_Test = "Physics";
    Scene scene("Physics");
    Entity floor = scene.createEntity("Floor");
    scene.get<Transform>(floor).scale = glm::vec3(20, 1, 20);
    scene.get<Transform>(floor).position = glm::vec3(0, -0.5f, 0);
    scene.add<BoxCollider>(floor);
    Entity wall = scene.createEntity("Wall");
    scene.get<Transform>(wall).position = glm::vec3(3, 1, 0);
    scene.add<BoxCollider>(wall);
    scene.updateTransforms();
    PhysicsWorld& physics = scene.physics();
    physics.rebuild(scene);

    RaycastHit hit;
    CHECK(physics.raycast(Ray{{0, 10, 0}, {0, -1, 0}}, 100.0f, hit));
    CHECK(hit.entity == floor && near(hit.point.y, 0.0f) && near(hit.normal, glm::vec3(0, 1, 0)));
    CHECK(physics.raycast(Ray{{0, 1, 0}, {1, 0, 0}}, 100.0f, hit) && hit.entity == wall && near(hit.distance, 2.5f));
    CHECK(!physics.raycast(Ray{{0, 1, 0}, {1, 0, 0}}, 100.0f, hit, wall) || hit.entity != wall);

    // A falling box lands on the floor and is stopped by the wall.
    glm::vec3 half(0.3f, 0.9f, 0.3f);
    MoveResult fall = physics.moveBox({0, 3, 0}, half, {0, -10, 0});
    CHECK(fall.grounded && near(fall.position.y, 0.9f, 0.01f));
    MoveResult slide = physics.moveBox({0, 1, 0}, half, {10, 0, 0});
    CHECK(slide.blocked.x && slide.position.x < 2.5f - 0.29f);

    // Rigid bodies fall under gravity when the scene plays.
    Entity crate = scene.createEntity("Crate");
    scene.get<Transform>(crate).position = glm::vec3(-3, 5, 0);
    scene.add<BoxCollider>(crate);
    scene.add<RigidBody>(crate);
    scene.beginPlay();
    for (int i = 0; i < 240; ++i) scene.update(1.0f / 60.0f);
    CHECK(near(scene.get<Transform>(crate).position.y, 0.5f, 0.05f));
    scene.endPlay();
}

void writeFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary);
    f << text;
}

void testLoaders() {
    g_Test = "Loaders";
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (!glfwInit()) {
        std::printf("  (skipped: no GLFW)\n");
        return;
    }
    GLFWwindow* window = glfwCreateWindow(64, 64, "tests", nullptr, nullptr);
    if (!window) {
        std::printf("  (skipped: no OpenGL 3.3 context)\n");
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    gladLoadGL((GLADloadfunc)glfwGetProcAddress);

    // OBJ written on the fly: quad faces, negative indices, no normals, two materials.
    std::string dir = std::filesystem::temp_directory_path().generic_string();
    writeFile(dir + "/forge_test.mtl", "newmtl Red\nKd 1 0 0\nNs 100\nnewmtl Blue\nKd 0 0 1\n");
    writeFile(dir + "/forge_test.obj",
              "mtllib forge_test.mtl\n"
              "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
              "usemtl Red\nf 1 2 3 4\n"
              "v 0 0 1\nv 1 0 1\nv 1 1 1\n"
              "usemtl Blue\nf -3 -2 -1\n");
    Ref<Model> obj = loadOBJ(dir + "/forge_test.obj");
    CHECK(obj != nullptr);
    if (obj) {
        CHECK(obj->submeshes.size() == 2);
        CHECK(obj->materials.size() == 2);
        CHECK(near(obj->materials[0]->albedo.r, 1.0f) && near(obj->materials[1]->albedo.b, 1.0f));
        uint32_t tris = 0;
        for (auto& sm : obj->submeshes) tris += sm.mesh->indexCount() / 3;
        CHECK(tris == 3);
        CHECK(near(obj->submeshes[0].mesh->vertices()[0].normal, glm::vec3(0, 0, 1)));
    }

    Ref<Model> crate = AssetManager::getModel("models/crate.obj");
    CHECK(crate && crate->submeshes.size() == 1);
    if (crate) {
        CHECK(near(crate->bounds.min, glm::vec3(-0.5f, 0, -0.5f)) && near(crate->bounds.max, glm::vec3(0.5f, 1, 0.5f)));
        CHECK(crate->materials[0]->albedoTexture != nullptr);
    }

    Ref<Model> rifle = AssetManager::getModel("models/rifle.gltf");
    CHECK(rifle && rifle->submeshes.size() == 4);
    if (rifle) CHECK(rifle->bounds.min.z < -0.7f && rifle->bounds.max.z > 0.3f); // node transforms baked in

    Ref<Model> drone = AssetManager::getModel("models/drone.glb");
    CHECK(drone != nullptr);
    if (drone) {
        bool textured = false, emissive = false;
        for (auto& m : drone->materials) {
            textured |= m->albedoTexture != nullptr;
            emissive |= m->emission.r > 1.0f;
        }
        CHECK(textured); // PNG embedded in the GLB binary chunk
        CHECK(emissive); // KHR_materials_emissive_strength
    }

    // Weapon viewmodels: the muzzle-flash offsets in ArenaBuilder rely on these extents.
    struct WeaponModel {
        const char* path;
        float muzzleZ;
    };
    for (const WeaponModel& wm : {WeaponModel{"models/shotgun.glb", -0.67f}, WeaponModel{"models/sniper.glb", -0.85f},
                                  WeaponModel{"models/launcher.glb", -0.668f}}) {
        Ref<Model> m = AssetManager::getModel(wm.path);
        CHECK(m && m->submeshes.size() >= 4);
        if (m) CHECK(std::fabs(m->bounds.min.z - wm.muzzleZ) < 0.01f);
    }
    Ref<Model> rocket = AssetManager::getModel("models/rocket.obj");
    CHECK(rocket && rocket->submeshes.size() == 3);
    if (rocket) CHECK(rocket->bounds.min.z < -0.25f); // nose points down -Z (forward)

    Ref<Model> cube = AssetManager::getModel("builtin:Cube");
    CHECK(cube && cube->submeshes[0].mesh->indexCount() == 36);
    float t;
    CHECK(cube && cube->raycast(Ray{{0, 0, 5}, {0, 0, -1}}, t) && near(t, 4.5f));

    Ref<Material> floorMat = AssetManager::getMaterial("materials/Floor.mat");
    CHECK(floorMat && floorMat->worldSpaceUV && floorMat->albedoTexture);

    for (const char* shader : {"Lit", "Unlit", "Depth", "Prepass", "SSAO", "SSAOBlur", "Skybox", "Grid", "Lines", "Particles",
                               "BloomPrefilter", "Blur", "Composite", "FXAA", "Mask", "Outline"}) {
        Ref<Shader> s = AssetManager::getShader(shader);
        if (!s) std::printf("  shader %s failed\n", shader);
        CHECK(s && s->valid());
    }

    AssetManager::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}
} // namespace

int main(int argc, char** argv) {
    FileSystem::initAssetRoot(argc > 0 ? argv[0] : nullptr);
    struct { const char* name; void (*fn)(); } tests[] = {
        {"Registry", testRegistry}, {"Json", testJson}, {"Math", testMath},
        {"Scene serialization", testSceneSerialization}, {"Physics", testPhysics}, {"Model loaders", testLoaders},
    };
    for (auto& t : tests) {
        int failedBefore = g_Failed;
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        std::printf("[ %s ] %s\n", g_Failed == failedBefore ? " OK " : "FAIL", t.name);
    }
    std::printf("\n%d checks passed, %d failed\n", g_Passed, g_Failed);
    return g_Failed == 0 ? 0 : 1;
}
