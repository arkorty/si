#pragma once

#include "Game.h"
#include "Renderer.h"
#include "InputManager.h"
#include <vector>
#include <memory>
#include <string>

class Console {
public:
    Console();
    ~Console();

    bool init();
    void run();
    void shutdown();

    void registerGame(std::unique_ptr<IGame> game);

    void onKey(int key, int action);
    void onChar(unsigned int codepoint);
    void onResize(int width, int height);

private:
    void mainLoop();
    void update(float dt);
    void render();
    void handleInput();

    // Menu UI
    void renderMenu();
    void launchGame(int index);

    GLFWwindow* window = nullptr;
    GameContext ctx;

    Renderer renderer;
    InputManager inputManager;

    std::vector<std::unique_ptr<IGame>> games;
    int currentGame = -1;
    int selectedGame = 0;
    int viewportX = 0;
    int viewportY = 0;
    bool inMenu = true;
    bool shouldExit = false;

    double lastTime = 0.0;
};

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void charCallback(GLFWwindow* window, unsigned int codepoint);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
