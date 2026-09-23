#include "Console.h"
#include "games/SpaceInvaders.h"
#include <iostream>

Console::Console() {}

Console::~Console() {
    shutdown();
}

bool Console::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Request OpenGL 3.3 Core Profile - modern OpenGL, no fixed-function pipeline
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(800, 600, "Retro Console", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetWindowUserPointer(window, this);

    if (!renderer.init()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ctx.window = window;
    ctx.renderer = &renderer;
    ctx.input = &inputManager;
    ctx.windowWidth = w;
    ctx.windowHeight = h;
    ctx.aspectRatio = (float)w / (float)h;

    registerGame(std::make_unique<SpaceInvadersGame>());

    for (auto& game : games) {
        game->init(ctx);
    }

    lastTime = glfwGetTime();
    return true;
}

void Console::registerGame(std::unique_ptr<IGame> game) {
    games.push_back(std::move(game));
}

void Console::run() {
    mainLoop();
}

void Console::mainLoop() {
    while (!shouldExit && !glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float dt = (float)(currentTime - lastTime);
        lastTime = currentTime;
        if (dt > 0.1f) dt = 0.1f;

        ctx.currentTime = currentTime;
        ctx.deltaTime = dt;

        glfwPollEvents();

        handleInput();
        update(dt);
        render();

        // Must run AFTER input handling: copies current keys → prev keys
        // so isKeyPressed() can detect new presses next frame
        inputManager.update();

        glfwSwapBuffers(window);
    }
}

void Console::update(float dt) {
    if (inMenu) return;
    if (currentGame >= 0 && currentGame < (int)games.size()) {
        games[currentGame]->update(ctx, dt);
    }
}

void Console::render() {
    renderer.beginFrame();

    if (inMenu) {
        renderMenu();
    } else if (currentGame >= 0 && currentGame < (int)games.size()) {
        games[currentGame]->render(ctx);
    }

    renderer.endFrame();
}

void Console::handleInput() {
    if (inMenu) {
        if (inputManager.isKeyPressed(GLFW_KEY_UP) || inputManager.isKeyPressed(GLFW_KEY_W)) {
            selectedGame = (selectedGame - 1 + (int)games.size()) % (int)games.size();
        }
        if (inputManager.isKeyPressed(GLFW_KEY_DOWN) || inputManager.isKeyPressed(GLFW_KEY_S)) {
            selectedGame = (selectedGame + 1) % (int)games.size();
        }
        if (inputManager.isKeyPressed(GLFW_KEY_ENTER) || inputManager.isKeyPressed(GLFW_KEY_SPACE)) {
            if (!games.empty()) {
                inMenu = false;
                currentGame = selectedGame;
                games[currentGame]->onEnter(ctx);
            }
        }
        if (inputManager.isKeyPressed(GLFW_KEY_ESCAPE)) {
            shouldExit = true;
        }
    } else {
        if (inputManager.isKeyPressed(GLFW_KEY_ESCAPE)) {
            if (currentGame >= 0 && currentGame < (int)games.size()) {
                games[currentGame]->onExit(ctx);
            }
            inMenu = true;
            currentGame = -1;
        }
    }
}

void Console::renderMenu() {
    Renderer* r = &renderer;

    // Full-screen background
    r->drawRect(glm::vec2(-1.0f, -1.0f), glm::vec2(2.0f, 2.0f), glm::vec4(0.1f, 0.1f, 0.25f, 1.0f));

    // Title bar
    r->drawRect(glm::vec2(-0.7f, 0.7f), glm::vec2(1.4f, 0.15f), glm::vec4(0.8f, 0.6f, 0.1f, 1.0f));
    r->drawRect(glm::vec2(-0.65f, 0.725f), glm::vec2(1.3f, 0.1f), glm::vec4(0.2f, 0.15f, 0.4f, 1.0f));
    r->drawText(glm::vec2(-0.35f, 0.74f), "RETRO CONSOLE", 0.06f, glm::vec4(1, 1, 0.8f, 1));

    // Game list
    float startY = 0.3f;
    for (size_t i = 0; i < games.size(); ++i) {
        float y = startY - (float)i * 0.2f;
        bool selected = ((int)i == selectedGame);

        glm::vec4 bg = selected ? glm::vec4(0.4f, 0.3f, 0.6f, 1.0f)
                                : glm::vec4(0.2f, 0.15f, 0.35f, 1.0f);
        r->drawRect(glm::vec2(-0.6f, y - 0.06f), glm::vec2(1.2f, 0.12f), bg);

        if (selected) {
            // Selector marker
            r->drawRect(glm::vec2(-0.75f, y - 0.03f), glm::vec2(0.08f, 0.06f), glm::vec4(1.0f, 0.9f, 0.2f, 1.0f));
        }

        // Game name
        glm::vec4 textColor = selected ? glm::vec4(1, 1, 1, 1) : glm::vec4(0.7f, 0.7f, 0.85f, 1);
        r->drawText(glm::vec2(-0.5f, y - 0.025f), games[i]->getName(), 0.05f, textColor);
    }

    // Bottom hint bar
    r->drawRect(glm::vec2(-0.7f, -0.85f), glm::vec2(1.4f, 0.1f), glm::vec4(0.15f, 0.1f, 0.3f, 1.0f));
    r->drawText(glm::vec2(-0.55f, -0.82f), "ARROWS/WS = MOVE   ENTER = PLAY   ESC = QUIT",
                0.035f, glm::vec4(0.7f, 0.7f, 0.85f, 1));
}

void Console::onKey(int key, int action) {
    inputManager.setKey(key, action == GLFW_PRESS || action == GLFW_REPEAT);
}

void Console::onResize(int width, int height) {
    ctx.windowWidth = width;
    ctx.windowHeight = height;
    ctx.aspectRatio = (float)width / (float)height;
    glViewport(0, 0, width, height);
    renderer.setProjection(glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));

    for (auto& game : games) {
        game->onResize(ctx, width, height);
    }
}

void Console::shutdown() {
    for (auto& game : games) {
        game->cleanup(ctx);
    }
    games.clear();
    renderer.shutdown();

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    Console* console = static_cast<Console*>(glfwGetWindowUserPointer(window));
    if (console) {
        console->onKey(key, action);
    }
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Console* console = static_cast<Console*>(glfwGetWindowUserPointer(window));
    if (console) {
        console->onResize(width, height);
    }
}
