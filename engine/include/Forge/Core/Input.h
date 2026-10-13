#pragma once
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace Forge {

// Key codes (identical values to GLFW so they can be passed straight through).
namespace Key {
constexpr int Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47;
constexpr int D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52, D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57;
constexpr int A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74, K = 75, L = 76, M = 77;
constexpr int N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90;
constexpr int Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261;
constexpr int Right = 262, Left = 263, Down = 264, Up = 265;
constexpr int F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295, F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301;
constexpr int LeftShift = 340, LeftControl = 341, LeftAlt = 342, RightShift = 344, RightControl = 345, RightAlt = 346;
constexpr int Count = 349;
} // namespace Key

namespace Mouse {
constexpr int Left = 0, Right = 1, Middle = 2;
constexpr int Count = 8;
} // namespace Mouse

// Unity-style static input facade used by gameplay code.
// When input is disabled (e.g. the editor's Game view is not focused) every
// query returns "not pressed" so gameplay code never reacts to editor clicks.
class Input {
public:
    static bool getKey(int key);
    static bool getKeyDown(int key);
    static bool getKeyUp(int key);

    static bool getMouseButton(int button);
    static bool getMouseButtonDown(int button);
    static bool getMouseButtonUp(int button);

    static glm::vec2 mousePosition();
    static glm::vec2 mouseDelta();
    static float scrollDelta();
    // Ignores the enabled flag (used by editor tools such as the scene camera).
    static glm::vec2 rawMouseDelta();

    static void setCursorLocked(bool locked);
    static bool isCursorLocked();

    static void setEnabled(bool enabled);
    static bool isEnabled();

    // --- Engine internal -------------------------------------------------
    static void init(GLFWwindow* window);
    static void update(); // poll devices, called once per frame
    static void onScroll(double dy);
};

} // namespace Forge
