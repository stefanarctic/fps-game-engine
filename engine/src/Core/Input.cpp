#include "Forge/Core/Input.h"

#include <GLFW/glfw3.h>
#include <cstring>

namespace Forge {

namespace {
struct InputState {
    GLFWwindow* window = nullptr;
    bool keys[Key::Count] = {};
    bool prevKeys[Key::Count] = {};
    bool buttons[Mouse::Count] = {};
    bool prevButtons[Mouse::Count] = {};
    glm::vec2 mousePos{0.0f};
    glm::vec2 lastMousePos{0.0f};
    glm::vec2 mouseDelta{0.0f};
    float scrollAccum = 0.0f;
    float scroll = 0.0f;
    bool locked = false;
    bool enabled = true;
    bool firstUpdate = true;
};

InputState& S() {
    static InputState s;
    return s;
}

bool validKey(int key) { return key >= 0 && key < Key::Count; }
bool validButton(int b) { return b >= 0 && b < Mouse::Count; }
} // namespace

void Input::init(GLFWwindow* window) {
    S().window = window;
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Input::onScroll(double dy) { S().scrollAccum += (float)dy; }

void Input::update() {
    InputState& s = S();
    if (!s.window) return;

    std::memcpy(s.prevKeys, s.keys, sizeof(s.keys));
    std::memcpy(s.prevButtons, s.buttons, sizeof(s.buttons));

    for (int k = Key::Space; k < Key::Count; ++k)
        s.keys[k] = glfwGetKey(s.window, k) == GLFW_PRESS;
    for (int b = 0; b < Mouse::Count; ++b)
        s.buttons[b] = glfwGetMouseButton(s.window, b) == GLFW_PRESS;

    double x, y;
    glfwGetCursorPos(s.window, &x, &y);
    s.mousePos = glm::vec2((float)x, (float)y);
    if (s.firstUpdate) {
        s.lastMousePos = s.mousePos;
        s.firstUpdate = false;
    }
    s.mouseDelta = s.mousePos - s.lastMousePos;
    s.lastMousePos = s.mousePos;

    s.scroll = s.scrollAccum;
    s.scrollAccum = 0.0f;
}

bool Input::getKey(int key) { return S().enabled && validKey(key) && S().keys[key]; }
bool Input::getKeyDown(int key) { return S().enabled && validKey(key) && S().keys[key] && !S().prevKeys[key]; }
bool Input::getKeyUp(int key) { return S().enabled && validKey(key) && !S().keys[key] && S().prevKeys[key]; }

bool Input::getMouseButton(int b) { return S().enabled && validButton(b) && S().buttons[b]; }
bool Input::getMouseButtonDown(int b) { return S().enabled && validButton(b) && S().buttons[b] && !S().prevButtons[b]; }
bool Input::getMouseButtonUp(int b) { return S().enabled && validButton(b) && !S().buttons[b] && S().prevButtons[b]; }

glm::vec2 Input::mousePosition() { return S().mousePos; }
glm::vec2 Input::mouseDelta() { return S().enabled ? S().mouseDelta : glm::vec2(0.0f); }
glm::vec2 Input::rawMouseDelta() { return S().mouseDelta; }
float Input::scrollDelta() { return S().enabled ? S().scroll : 0.0f; }

void Input::setCursorLocked(bool locked) {
    InputState& s = S();
    if (s.locked == locked || !s.window) return;
    s.locked = locked;
    glfwSetInputMode(s.window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // Avoid a big delta spike on the frame the cursor mode changes.
    double x, y;
    glfwGetCursorPos(s.window, &x, &y);
    s.lastMousePos = s.mousePos = glm::vec2((float)x, (float)y);
    s.mouseDelta = glm::vec2(0.0f);
}

bool Input::isCursorLocked() { return S().locked; }
void Input::setEnabled(bool enabled) { S().enabled = enabled; }
bool Input::isEnabled() { return S().enabled; }

} // namespace Forge
