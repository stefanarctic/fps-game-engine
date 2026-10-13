#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

class Scene;

// Systems hold the behaviour; components hold only data.
// A Scene owns its systems and runs them every frame, ordered by `priority`
// (lower runs first). By default systems only run while the scene is playing;
// set `runInEditMode` for systems that must also tick in the editor.
class System {
public:
    virtual ~System() = default;

    virtual const char* name() const = 0;
    virtual void onStart(Scene& scene) { FORGE_UNUSED(scene); }   // play mode entered
    virtual void onUpdate(Scene& scene, float dt) = 0;
    virtual void onStop(Scene& scene) { FORGE_UNUSED(scene); }    // play mode exited

    int priority = 0;
    bool runInEditMode = false;
    bool enabled = true;
};

// Engine system priorities. Gameplay systems default to 0.
namespace SystemPriority {
constexpr int Gameplay = 0;
constexpr int Physics = 100;
constexpr int Particles = 200;
constexpr int Lifetime = 300;
} // namespace SystemPriority

} // namespace Forge
