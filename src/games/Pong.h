#pragma once

#include "Game.h"
#include "Renderer.h"
#include "InputManager.h"
#include <random>

class PongGame : public IGame {
public:
    PongGame();
    ~PongGame() override = default;

    const char* getName() const override { return "Pong"; }
    const char* getDescription() const override { return "Classic paddle duel"; }

    void init(GameContext& ctx) override;
    void cleanup(GameContext& ctx) override;
    void onEnter(GameContext& ctx) override;
    void onExit(GameContext& ctx) override;
    void update(GameContext& ctx, float dt) override;
    void render(GameContext& ctx) override;
    void onKey(GameContext& ctx, int key, int action) override;
    void onResize(GameContext& ctx, int width, int height) override;

private:
    void resetBall();
    void resetGame();
    void updateAI(float dt);

    // Paddles: x is fixed, y is center
    float leftY, rightY;
    float paddleH, paddleW;
    float leftX, rightX;

    // Ball
    float ballX, ballY;
    float ballVX, ballVY;
    float ballSize;
    bool ballMoving;

    // Scoring
    int leftScore, rightScore;
    int winScore;
    bool gameOver;
    bool leftWon;
    float serveDelay;

    // AI mode: right paddle controlled by computer
    bool vsAI;
    float aiReaction; // current AI tracking offset (simulates imperfection)
    float aiReactionTarget;

    std::mt19937 rng;
};
