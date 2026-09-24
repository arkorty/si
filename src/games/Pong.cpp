#include "Pong.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

constexpr float PADDLE_SPEED = 0.9f;
constexpr float BALL_SPEED_INIT = 0.7f;
constexpr float BALL_SPEED_INC = 0.05f;
constexpr float BALL_MAX_SPEED = 1.5f;
constexpr float AI_SPEED = 0.75f; // slightly slower than human

PongGame::PongGame()
    : leftY(0), rightY(0), paddleH(0.2f), paddleW(0.025f),
      leftX(-0.9f), rightX(0.9f),
      ballX(0), ballY(0), ballVX(0), ballVY(0), ballSize(0.025f),
      ballMoving(false), leftScore(0), rightScore(0), winScore(7),
      gameOver(false), leftWon(false), serveDelay(0),
      vsAI(true), aiReaction(0), aiReactionTarget(0),
      rng(std::random_device{}()) {}

void PongGame::init(GameContext& ctx) { (void)ctx; resetGame(); }
void PongGame::cleanup(GameContext& ctx) { (void)ctx; }
void PongGame::onEnter(GameContext& ctx) { (void)ctx; resetGame(); }
void PongGame::onExit(GameContext& ctx) { (void)ctx; }
void PongGame::onResize(GameContext& ctx, int w, int h) { (void)ctx; (void)w; (void)h; }

void PongGame::onKey(GameContext& ctx, int key, int action) {
    (void)ctx;
    if (action == GLFW_PRESS && key == GLFW_KEY_T) {
        vsAI = !vsAI;
        // New random imperfection each mode switch
        std::uniform_real_distribution<float> d(-0.08f, 0.08f);
        aiReactionTarget = d(rng);
    }
}

void PongGame::resetBall() {
    ballX = 0;
    ballY = 0;
    ballMoving = false;
    serveDelay = 1.5f;

    std::uniform_real_distribution<float> angle(-0.6f, 0.6f);
    std::uniform_real_distribution<float> sign(-1.0f, 1.0f);
    float dir = sign(rng) > 0 ? 1.0f : -1.0f;
    float a = angle(rng);
    ballVX = dir * BALL_SPEED_INIT * std::cos(a);
    ballVY = BALL_SPEED_INIT * std::sin(a);
}

void PongGame::resetGame() {
    leftY = 0;
    rightY = 0;
    leftScore = 0;
    rightScore = 0;
    gameOver = false;
    leftWon = false;

    // Random AI imperfection so it's not perfect
    std::uniform_real_distribution<float> d(-0.1f, 0.1f);
    aiReactionTarget = d(rng);
    aiReaction = 0;

    resetBall();
}

void PongGame::updateAI(float dt) {
    // AI tracks the ball but with imperfection (reaction offset)
    float targetY = ballY + aiReaction;

    // Only react to ball moving toward AI or near center
    if (ballVX < 0 || ballX < 0) {
        float diff = targetY - leftY;
        // Dead zone so AI doesn't jitter
        if (std::fabs(diff) > 0.03f) {
            float move = (diff > 0 ? 1.0f : -1.0f) * AI_SPEED * dt;
            // Don't overshoot
            if (std::fabs(move) > std::fabs(diff)) move = diff;
            leftY += move;
        }
    } else {
        // Ball moving away — drift back to center slowly
        float diff = 0.0f - leftY;
        if (std::fabs(diff) > 0.05f) {
            leftY += (diff > 0 ? 1.0f : -1.0f) * AI_SPEED * 0.3f * dt;
        }
    }

    // Occasionally update reaction offset (makes AI feel human)
    static float reactionTimer = 0;
    reactionTimer += dt;
    if (reactionTimer > 2.0f) {
        reactionTimer = 0;
        std::uniform_real_distribution<float> d(-0.1f, 0.1f);
        aiReactionTarget = d(rng);
    }
    // Smoothly move toward target reaction
    aiReaction += (aiReactionTarget - aiReaction) * dt * 2.0f;
}

void PongGame::update(GameContext& ctx, float dt) {
    if (ctx.input->isKeyPressed(GLFW_KEY_R) && gameOver) {
        resetGame();
        return;
    }
    if (gameOver) return;

    if (vsAI) {
        updateAI(dt);
        if (ctx.input->isKeyDown(GLFW_KEY_UP)) rightY += PADDLE_SPEED * dt;
        if (ctx.input->isKeyDown(GLFW_KEY_DOWN)) rightY -= PADDLE_SPEED * dt;
    } else {
        // Player 1 (left) - W/S
        if (ctx.input->isKeyDown(GLFW_KEY_W)) leftY += PADDLE_SPEED * dt;
        if (ctx.input->isKeyDown(GLFW_KEY_S)) leftY -= PADDLE_SPEED * dt;
        
        // Player 2 (right) - Arrows
        if (ctx.input->isKeyDown(GLFW_KEY_UP)) rightY += PADDLE_SPEED * dt;
        if (ctx.input->isKeyDown(GLFW_KEY_DOWN)) rightY -= PADDLE_SPEED * dt;
    }

    // Clamp paddles
    float halfPH = paddleH * 0.5f;
    leftY = std::max(-1.0f + halfPH, std::min(1.0f - halfPH, leftY));
    rightY = std::max(-1.0f + halfPH, std::min(1.0f - halfPH, rightY));

    // Serve delay
    if (!ballMoving) {
        serveDelay -= dt;
        if (serveDelay <= 0) ballMoving = true;
        return;
    }

    // Move ball
    ballX += ballVX * dt;
    ballY += ballVY * dt;

    // Top/bottom bounce
    if (ballY + ballSize * 0.5f >= 1.0f) {
        ballY = 1.0f - ballSize * 0.5f;
        ballVY = -ballVY;
    }
    if (ballY - ballSize * 0.5f <= -1.0f) {
        ballY = -1.0f + ballSize * 0.5f;
        ballVY = -ballVY;
    }

    // Left paddle collision
    if (ballVX < 0 &&
        ballX - ballSize * 0.5f <= leftX + paddleW * 0.5f &&
        ballX - ballSize * 0.5f >= leftX - paddleW &&
        std::fabs(ballY - leftY) <= halfPH + ballSize * 0.5f) {
        float offset = (ballY - leftY) / halfPH;
        float speed = std::min(BALL_MAX_SPEED,
            std::sqrt(ballVX * ballVX + ballVY * ballVY) + BALL_SPEED_INC);
        float angle = offset * 1.0f;
        ballVX = speed * std::cos(angle);
        ballVY = speed * std::sin(angle);
        ballX = leftX + paddleW * 0.5f + ballSize * 0.5f;
    }

    // Right paddle collision
    if (ballVX > 0 &&
        ballX + ballSize * 0.5f >= rightX - paddleW * 0.5f &&
        ballX + ballSize * 0.5f <= rightX + paddleW &&
        std::fabs(ballY - rightY) <= halfPH + ballSize * 0.5f) {
        float offset = (ballY - rightY) / halfPH;
        float speed = std::min(BALL_MAX_SPEED,
            std::sqrt(ballVX * ballVX + ballVY * ballVY) + BALL_SPEED_INC);
        float angle = 3.14159f - offset * 1.0f;
        ballVX = speed * std::cos(angle);
        ballVY = speed * std::sin(angle);
        ballX = rightX - paddleW * 0.5f - ballSize * 0.5f;
    }

    // Scoring
    if (ballX < -1.1f) {
        rightScore++;
        if (rightScore >= winScore) { gameOver = true; leftWon = false; }
        else resetBall();
    }
    if (ballX > 1.1f) {
        leftScore++;
        if (leftScore >= winScore) { gameOver = true; leftWon = true; }
        else resetBall();
    }
}

void PongGame::render(GameContext& ctx) {
    Renderer* r = ctx.renderer;

    float halfPH = paddleH * 0.5f;

    // Center line (dashed)
    for (float y = -0.95f; y < 1.0f; y += 0.1f) {
        r->drawRect(glm::vec2(-0.005f, y), glm::vec2(0.01f, 0.05f),
                    glm::vec4(0.3f, 0.3f, 0.5f, 0.6f));
    }

    // Left paddle
    r->drawRect(glm::vec2(leftX - paddleW * 0.5f, leftY - halfPH),
                glm::vec2(paddleW, paddleH), glm::vec4(0.3f, 1, 0.5f, 1));

    // Right paddle
    r->drawRect(glm::vec2(rightX - paddleW * 0.5f, rightY - halfPH),
                glm::vec2(paddleW, paddleH), glm::vec4(1, 0.4f, 0.3f, 1));

    // Ball
    r->drawRect(glm::vec2(ballX - ballSize * 0.5f, ballY - ballSize * 0.5f),
                glm::vec2(ballSize, ballSize), glm::vec4(1, 1, 1, 1));

    // Scores
    char buf[64];
    snprintf(buf, sizeof(buf), "%d", leftScore);
    r->drawText(glm::vec2(-0.3f, 0.75f), buf, 0.1f, glm::vec4(0.3f, 1, 0.5f, 1));
    snprintf(buf, sizeof(buf), "%d", rightScore);
    r->drawText(glm::vec2(0.22f, 0.75f), buf, 0.1f, glm::vec4(1, 0.4f, 0.3f, 1));

    // Mode indicator
    const char* mode = vsAI ? "1P VS CPU" : "2P VERSUS";
    r->drawText(glm::vec2(-0.15f, 0.87f), mode, 0.04f,
                glm::vec4(0.6f, 0.6f, 0.8f, 1));
    r->drawText(glm::vec2(-0.15f, -0.95f), "T = TOGGLE MODE", 0.035f,
                glm::vec4(0.4f, 0.4f, 0.55f, 1));

    // Controls
    if (vsAI) {
        r->drawText(glm::vec2(-0.95f, -0.95f), "CPU", 0.04f, glm::vec4(0.3f, 1, 0.5f, 0.7f));
    } else {
        r->drawText(glm::vec2(-0.95f, -0.95f), "W/S", 0.04f, glm::vec4(0.3f, 1, 0.5f, 0.7f));
    }
    r->drawText(glm::vec2(0.65f, -0.95f), "UP/DOWN", 0.04f, glm::vec4(1, 0.4f, 0.3f, 0.7f));

    // Serve indicator
    if (!ballMoving && !gameOver) {
        r->drawText(glm::vec2(-0.3f, -0.1f), "SERVE IN...", 0.05f,
                    glm::vec4(0.7f, 0.7f, 0.9f, 1));
    }

    // Game over
    if (gameOver) {
        r->drawRect(glm::vec2(-1, -1), glm::vec2(2, 2), glm::vec4(0, 0, 0.2f, 0.75f));
        if (leftWon) {
            r->drawText(glm::vec2(-0.3f, 0.05f), "LEFT WINS!", 0.1f, glm::vec4(0.3f, 1, 0.5f, 1));
        } else {
            const char* msg = vsAI ? "CPU WINS!" : "RIGHT WINS!";
            r->drawText(glm::vec2(-0.3f, 0.05f), msg, 0.1f, glm::vec4(1, 0.4f, 0.3f, 1));
        }
        r->drawText(glm::vec2(-0.3f, -0.2f), "PRESS R TO RESTART", 0.05f, glm::vec4(0.8f, 0.8f, 0.8f, 1));
        r->drawText(glm::vec2(-0.3f, -0.3f), "ESC = MENU", 0.05f, glm::vec4(0.6f, 0.6f, 0.7f, 1));
    }
}
