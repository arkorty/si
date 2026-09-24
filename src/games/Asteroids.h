#pragma once

#include "Game.h"
#include "Renderer.h"
#include "InputManager.h"
#include <vector>
#include <random>

struct AVec2 {
    float x, y;
    AVec2(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
    AVec2 operator+(const AVec2& o) const { return AVec2(x + o.x, y + o.y); }
    AVec2 operator-(const AVec2& o) const { return AVec2(x - o.x, y - o.y); }
    AVec2 operator*(float s) const { return AVec2(x * s, y * s); }
    AVec2& operator+=(const AVec2& o) { x += o.x; y += o.y; return *this; }
    float length() const { return x * x + y * y; }
    AVec2 normalized() const {
        float len = std::sqrt(x * x + y * y);
        return len > 0 ? AVec2(x / len, y / len) : AVec2(0, 0);
    }
};

struct AsteroidParticle {
    AVec2 pos;
    AVec2 vel;
    float life;
    float maxLife;
    glm::vec4 color;
};

struct BulletA {
    AVec2 pos;
    AVec2 vel;
    bool active;
    float life;
};

struct Asteroid {
    AVec2 pos;
    AVec2 vel;
    float radius;
    int size; // 3=large, 2=medium, 1=small
    bool alive;
    float rotation;
    float rotSpeed;
    // Pre-computed shape vertices for drawing
    std::vector<AVec2> shape;
};

class AsteroidsGame : public IGame {
public:
    AsteroidsGame();
    ~AsteroidsGame() override = default;

    const char* getName() const override { return "Asteroids"; }
    const char* getDescription() const override { return "Navigate and destroy rocks"; }

    void init(GameContext& ctx) override;
    void cleanup(GameContext& ctx) override;
    void onEnter(GameContext& ctx) override;
    void onExit(GameContext& ctx) override;
    void update(GameContext& ctx, float dt) override;
    void render(GameContext& ctx) override;
    void onKey(GameContext& ctx, int key, int action) override;
    void onResize(GameContext& ctx, int width, int height) override;

private:
    void resetGame();
    void spawnAsteroid(int size, AVec2 pos);
    void spawnLargeAsteroids();
    void wrap(AVec2& p);
    void updatePlayer(float dt, GameContext& ctx);
    void updateAsteroids(float dt);
    void updateBullets(float dt);
    void updateParticles(float dt);
    void checkCollisions();
    void spawnExplosion(const AVec2& pos, const glm::vec4& color, int count);
    void splitAsteroid(Asteroid& a);
    void drawShip(Renderer* r);
    void drawAsteroids(Renderer* r);
    void drawBullets(Renderer* r);
    void drawParticles(Renderer* r);
    void drawUI(Renderer* r);
    int aliveAsteroids();

    // Player ship
    AVec2 shipPos;
    AVec2 shipVel;
    float shipAngle; // radians
    bool shipAlive;
    bool thrusting;
    float flamePhase;
    float shipInvincible; // seconds of invincibility after respawn
    float shootCooldown;

    std::vector<BulletA> bullets;
    std::vector<Asteroid> asteroids;
    std::vector<AsteroidParticle> particles;

    int score;
    int lives;
    int level;
    bool gameOver;
    bool paused;
    float respawnTimer;

    std::mt19937 rng;
};
