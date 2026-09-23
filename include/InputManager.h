#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <array>

class Renderer;
class InputManager;

struct GameContext {
    GLFWwindow* window = nullptr;
    Renderer* renderer = nullptr;
    InputManager* input = nullptr;

    int windowWidth = 800;
    int windowHeight = 600;
    float aspectRatio = 800.0f / 600.0f;

    double currentTime = 0.0;
    float deltaTime = 0.0f;

    bool shouldClose = false;
    bool fullscreen = false;
};

class InputManager {
public:
    static constexpr int MAX_KEYS = 1024;
    static constexpr int MAX_MOUSE_BUTTONS = 8;

    InputManager() {
        keys.fill(false);
        keysPrev.fill(false);
        mouseButtons.fill(false);
        mouseButtonsPrev.fill(false);
        mouseX = 0.0;
        mouseY = 0.0;
    }

    void update() {
        keysPrev = keys;
        mouseButtonsPrev = mouseButtons;
    }

    void setKey(int key, bool pressed) {
        if (key >= 0 && key < MAX_KEYS) keys[key] = pressed;
    }

    void setMouseButton(int button, bool pressed) {
        if (button >= 0 && button < MAX_MOUSE_BUTTONS) mouseButtons[button] = pressed;
    }

    void setMousePosition(double x, double y) {
        mouseX = x;
        mouseY = y;
    }

    bool isKeyDown(int key) const {
        if (key < 0 || key >= MAX_KEYS) return false;
        return keys[key];
    }

    bool isKeyPressed(int key) const {
        if (key < 0 || key >= MAX_KEYS) return false;
        return keys[key] && !keysPrev[key];
    }

    bool isKeyReleased(int key) const {
        if (key < 0 || key >= MAX_KEYS) return false;
        return !keys[key] && keysPrev[key];
    }

    bool isMouseButtonDown(int button) const {
        if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
        return mouseButtons[button];
    }

    bool isMouseButtonPressed(int button) const {
        if (button < 0 || button >= MAX_MOUSE_BUTTONS) return false;
        return mouseButtons[button] && !mouseButtonsPrev[button];
    }

    double getMouseX() const { return mouseX; }
    double getMouseY() const { return mouseY; }

private:
    std::array<bool, MAX_KEYS> keys;
    std::array<bool, MAX_KEYS> keysPrev;
    std::array<bool, MAX_MOUSE_BUTTONS> mouseButtons;
    std::array<bool, MAX_MOUSE_BUTTONS> mouseButtonsPrev;
    double mouseX, mouseY;
};