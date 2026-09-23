#pragma once

#include "Game.h"
#include "Renderer.h"
#include "InputManager.h"
#include <vector>
#include <random>

struct Vec2 {
    float x, y;
    Vec2(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
};

struct Rect {
    Vec2 pos;
    Vec2 size;
    Rect() = default;
    Rect(Vec2 p, Vec2 s) : pos(p), size(s) {}
    bool intersects(const Rect& o) const {
        return pos.x < o.pos.x + o.size.x && pos.x + size.x > o.pos.x &&
               pos.y < o.pos.y + o.size.y && pos.y + size.y > o.pos.y;
    }
};

struct Player {
    Vec2 pos;
    Vec2 size;
    bool alive;
    float shootCooldown;
    Player() : pos(0, -0.8f), size(0.12f, 0.05f), alive(true), shootCooldown(0) {}
    Rect getRect() const { return Rect(pos - size * 0.5f, size); }
};

struct Bullet {
    Vec2 pos;
    Vec2 vel;
    Vec2 size;
    bool active;
    bool isPlayerBullet;
    Bullet() : pos(0, 0), vel(0, 0), size(0.02f, 0.1f), active(false), isPlayerBullet(true) {}
    Rect getRect() const { return Rect(pos - size * 0.5f, size); }
};

struct Enemy {
    Vec2 pos;
    Vec2 size;
    bool alive;
    int type;
    float animTimer;
    Enemy() : pos(0, 0), size(0.08f, 0.06f), alive(false), type(0), animTimer(0) {}
    Rect getRect() const { return Rect(pos - size * 0.5f, size); }
};

struct Particle {
    Vec2 pos;
    Vec2 vel;
    Vec2 size;
    glm::vec4 color;
    float life;
    float maxLife;
    Particle() : pos(0, 0), vel(0, 0), size(0.02f, 0.02f), color(1, 1, 1, 1), life(0), maxLife(0) {}
};

class SpaceInvadersGame : public IGame {
public:
    SpaceInvadersGame();
    ~SpaceInvadersGame() override = default;

    const char* getName() const override { return "Space Invaders"; }
    const char* getDescription() const override { return "Classic arcade shooter"; }

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
    void initEnemies();
    void spawnPlayerBullet();
    void spawnEnemyBullet();
    void spawnExplosion(const Vec2& pos, const glm::vec4& color, int count);
    void updateBullets(float dt);
    void updateEnemies(float dt);
    void updateParticles(float dt);
    void checkCollisions();
    bool allEnemiesDead();
    void drawGame(Renderer* r);
    void drawUI(Renderer* r);

    Player player;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    std::vector<Particle> particles;

    float enemyDirection;
    bool enemyMoveDown;
    float enemyMoveTimer;
    float enemyShootTimer;
    float enemySpeed;

    int score;
    int lives;
    int wave;
    bool gameOver;
    bool victory;
    bool paused;

    std::mt19937 rng;
};
