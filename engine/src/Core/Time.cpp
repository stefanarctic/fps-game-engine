#include "Forge/Core/Time.h"

namespace Forge {

float Time::s_DeltaTime = 1.0f / 60.0f;
float Time::s_TimeScale = 1.0f;
double Time::s_Time = 0.0;
double Time::s_LastTime = -1.0;
uint64_t Time::s_FrameCount = 0;
float Time::s_Fps = 0.0f;
float Time::s_FpsAccum = 0.0f;
int Time::s_FpsFrames = 0;

void Time::tick(double now) {
    if (s_LastTime < 0.0) s_LastTime = now;
    double dt = now - s_LastTime;
    s_LastTime = now;
    // Clamp huge frame spikes (window drags, breakpoints) so simulation stays stable.
    if (dt > 0.1) dt = 0.1;
    if (dt < 0.0) dt = 0.0;
    s_DeltaTime = (float)dt;
    s_Time = now;
    s_FrameCount++;

    s_FpsAccum += (float)dt;
    s_FpsFrames++;
    if (s_FpsAccum >= 0.5f) {
        s_Fps = (float)s_FpsFrames / s_FpsAccum;
        s_FpsAccum = 0.0f;
        s_FpsFrames = 0;
    }
}

} // namespace Forge
