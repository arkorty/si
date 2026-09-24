#include "Console.h"
#include "games/SpaceInvaders.h"
#include "games/Asteroids.h"
#include "games/Pong.h"
#include "games/Spaceship.h"
#include <iostream>
#include <cmath>
#include <algorithm>

static const char* WINDOW_NAME = "si virtual console";

static std::string toUpper(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

Console::Console() {}

Console::~Console() {
    shutdown();
}

bool Console::init() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(800, 600, WINDOW_NAME, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
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
    registerGame(std::make_unique<AsteroidsGame>());
    registerGame(std::make_unique<PongGame>());
    registerGame(std::make_unique<SpaceshipGame>());

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
    // Clear the ENTIRE window to black for the letterbox bars
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable scissor test so the game/renderer only draws/clears inside the viewport
    glEnable(GL_SCISSOR_TEST);
    glScissor(viewportX, viewportY, ctx.windowWidth, ctx.windowHeight);

    renderer.beginFrame();

    if (inMenu) {
        renderMenu();
    } else if (currentGame >= 0 && currentGame < (int)games.size()) {
        games[currentGame]->render(ctx);
    }

    renderer.endFrame();
    
    // Disable scissor for the next frame
    glDisable(GL_SCISSOR_TEST);
}

void Console::handleInput() {
    if (inMenu) return; 

    if (inputManager.isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (currentGame >= 0 && currentGame < (int)games.size()) {
            games[currentGame]->onExit(ctx);
        }
        inMenu = true;
        currentGame = -1;
    }
}

void Console::launchGame(int index) {
    inMenu = false;
    currentGame = index;
    games[currentGame]->onEnter(ctx);
}

void Console::renderMenu() {
    Renderer* r = &renderer;
    float t = (float)ctx.currentTime;

    const glm::vec4 bgColor(0.02f, 0.02f, 0.05f, 1.0f);     // Very dark blue screen
    const glm::vec4 titleColor(0.9f, 0.2f, 0.2f, 1.0f);     // Red title
    const glm::vec4 normalColor(0.7f, 0.7f, 0.7f, 1.0f);    // Grey text
    const glm::vec4 highlightColor(1.0f, 0.8f, 0.0f, 1.0f); // Gold/Yellow highlight

    // Full background
    r->drawRect(glm::vec2(-1, -1), glm::vec2(2, 2), bgColor);

    // Subtle scanlines over the entire screen
    for (float y = -1.0f; y < 1.0f; y += 0.02f) {
        r->drawRect(glm::vec2(-1.0f, y), glm::vec2(2.0f, 0.005f), glm::vec4(0.0f, 0.0f, 0.0f, 0.2f));
    }

    const float charSize = 0.06f;
    auto getTextWidth = [charSize](const std::string& str, float sizeOverride = -1.0f) {
        float size = sizeOverride < 0.0f ? charSize : sizeOverride;
        return str.size() * size * (6.0f / 7.0f);
    };
    
    // Title
    std::string titleStr = "SI VIRTUAL CONSOLE";
    float titleSize = charSize + 0.02f;
    r->drawText(glm::vec2(-getTextWidth(titleStr, titleSize) * 0.5f, 0.6f), titleStr, titleSize, titleColor);

    std::string subtitleStr = "SELECT A GAME";
    float subSize = charSize - 0.01f;
    r->drawText(glm::vec2(-getTextWidth(subtitleStr, subSize) * 0.5f, 0.45f), subtitleStr, subSize, normalColor);

    // List games
    float startY = 0.2f;
    float spacing = 0.15f;

    for (int i = 0; i < (int)games.size(); ++i) {
        std::string name = toUpper(games[i]->getName());
        float y = startY - i * spacing;
        float nameWidth = getTextWidth(name, charSize);
        float nameX = -nameWidth * 0.5f;
        
        if (i == selectedGame) {
            // Blinking selection cursor
            bool blink = ((int)(t * 4.0f) % 2) == 0;
            if (blink) {
                float cursorWidth = getTextWidth(">", charSize);
                r->drawText(glm::vec2(nameX - cursorWidth - 0.05f, y), ">", charSize, highlightColor);
                r->drawText(glm::vec2(nameX + nameWidth + 0.05f, y), "<", charSize, highlightColor);
            }
            r->drawText(glm::vec2(nameX, y), name, charSize, highlightColor);
        } else {
            r->drawText(glm::vec2(nameX, y), name, charSize, normalColor);
        }
    }

    // Instructions
    std::string instStr = "USE ARROWS TO SELECT - ENTER TO PLAY";
    float instSize = 0.035f;
    r->drawText(glm::vec2(-getTextWidth(instStr, instSize) * 0.5f, -0.7f), instStr, instSize, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

    // Soft vignette corners (over the entire screen)
    r->drawRect(glm::vec2(-1, 0.96f), glm::vec2(2, 0.04f), glm::vec4(0, 0, 0, 0.4f));
    r->drawRect(glm::vec2(-1, -1), glm::vec2(2, 0.04f), glm::vec4(0, 0, 0, 0.4f));
    r->drawRect(glm::vec2(-1, -1), glm::vec2(0.04f, 2), glm::vec4(0, 0, 0, 0.4f));
    r->drawRect(glm::vec2(0.96f, -1), glm::vec2(0.04f, 2), glm::vec4(0, 0, 0, 0.4f));
}

// --- Input ---

void Console::onKey(int key, int action) {
    inputManager.setKey(key, action == GLFW_PRESS || action == GLFW_REPEAT);

    if (!inMenu) {
        if (currentGame >= 0 && currentGame < (int)games.size()) {
            games[currentGame]->onKey(ctx, key, action);
        }
        return;
    }

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (key == GLFW_KEY_UP) {
        selectedGame--;
        if (selectedGame < 0) selectedGame = (int)games.size() - 1;
    } else if (key == GLFW_KEY_DOWN) {
        selectedGame++;
        if (selectedGame >= (int)games.size()) selectedGame = 0;
    } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE) {
        if (games.size() > 0) {
            launchGame(selectedGame);
        }
    } else if (key == GLFW_KEY_ESCAPE) {
        shouldExit = true;
    }
}

void Console::onChar(unsigned int codepoint) {
    // No longer a text shell
    (void)codepoint;
}

void Console::onResize(int width, int height) {
    float targetAspect = 4.0f / 3.0f;
    float windowAspect = (float)width / (float)height;

    int viewWidth = width;
    int viewHeight = height;
    viewportX = 0;
    viewportY = 0;

    if (windowAspect > targetAspect) {
        // Window is too wide (pillarbox)
        viewWidth = (int)(height * targetAspect);
        viewportX = (width - viewWidth) / 2;
    } else {
        // Window is too tall (letterbox)
        viewHeight = (int)(width / targetAspect);
        viewportY = (height - viewHeight) / 2;
    }

    ctx.windowWidth = viewWidth;
    ctx.windowHeight = viewHeight;
    ctx.aspectRatio = targetAspect;
    glViewport(viewportX, viewportY, viewWidth, viewHeight);
    renderer.setProjection(glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f));

    for (auto& game : games) {
        game->onResize(ctx, viewWidth, viewHeight);
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

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    Console* console = static_cast<Console*>(glfwGetWindowUserPointer(window));
    if (console) {
        console->onChar(codepoint);
    }
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Console* console = static_cast<Console*>(glfwGetWindowUserPointer(window));
    if (console) {
        console->onResize(width, height);
    }
}
