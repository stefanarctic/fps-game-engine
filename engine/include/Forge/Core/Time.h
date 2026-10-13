#pragma once
#include <cstdint>

namespace Forge {

// Unity-style static time access. Updated once per frame by the Application.
class Time {
public:
    static float deltaTime() { return s_DeltaTime * s_TimeScale; }
    static float unscaledDeltaTime() { return s_DeltaTime; }
    static double time() { return s_Time; }          // seconds since start (unscaled)
    static uint64_t frameCount() { return s_FrameCount; }
    static float fps() { return s_Fps; }

    static float timeScale() { return s_TimeScale; }
    static void setTimeScale(float scale) { s_TimeScale = scale < 0.0f ? 0.0f : scale; }

    // Called by the Application at the start of every frame.
    static void tick(double now);

private:
    static float s_DeltaTime;
    static float s_TimeScale;
    static double s_Time;
    static double s_LastTime;
    static uint64_t s_FrameCount;
    static float s_Fps;
    static float s_FpsAccum;
    static int s_FpsFrames;
};

} // namespace Forge
