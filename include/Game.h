#pragma once

#include <string>

struct GameContext;

class IGame {
public:
    virtual ~IGame() = default;

    virtual const char* getName() const = 0;
    virtual const char* getDescription() const = 0;

    virtual void init(GameContext& ctx) = 0;
    virtual void cleanup(GameContext& ctx) = 0;

    virtual void onEnter(GameContext& ctx) = 0;
    virtual void onExit(GameContext& ctx) = 0;

    virtual void update(GameContext& ctx, float dt) = 0;
    virtual void render(GameContext& ctx) = 0;

    virtual void onKey(GameContext& ctx, int key, int action) = 0;
    virtual void onResize(GameContext& ctx, int width, int height) = 0;
};