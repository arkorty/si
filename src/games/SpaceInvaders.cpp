#include "SpaceInvaders.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// Grid layout for the enemy formation
constexpr int ENEMY_ROWS = 5;
constexpr int ENEMY_COLS = 11;
constexpr float ENEMY_SPACING_X = 0.11f;
constexpr float ENEMY_SPACING_Y = 0.1f;
constexpr float ENEMY_START_X = -0.55f;
constexpr float ENEMY_START_Y = 0.7f;

constexpr float PLAYER_SPEED = 0.8f;
constexpr float BULLET_SPEED = 2.0f;       // fast bullets
constexpr float ENEMY_BASE_SPEED = 0.12f;
constexpr float ENEMY_DROP = 0.05f;
constexpr float SHOOT_COOLDOWN = 0.25f;
constexpr float ENEMY_SHOOT_INTERVAL = 1.5f;

SpaceInvadersGame::SpaceInvadersGame()
    : enemyDirection(1.0f), enemyMoveDown(false), enemyMoveTimer(0), enemyShootTimer(0),
      enemySpeed(ENEMY_BASE_SPEED), score(0), lives(3), wave(1),
      gameOver(false), victory(false), paused(false),
      rng(std::random_device{}()) {
    bullets.reserve(50);
    particles.reserve(200);
    enemies.resize(ENEMY_ROWS * ENEMY_COLS);
    bullets.resize(50); // resize, not reserve — need actual elements to reuse
}

void SpaceInvadersGame::init(GameContext& ctx) {
    (void)ctx;
    resetGame();
}

void SpaceInvadersGame::cleanup(GameContext& ctx) { (void)ctx; }
void SpaceInvadersGame::onEnter(GameContext& ctx) { (void)ctx; resetGame(); }
void SpaceInvadersGame::onExit(GameContext& ctx) { (void)ctx; }
void SpaceInvadersGame::onKey(GameContext& ctx, int key, int action) { (void)ctx; (void)key; (void)action; }
void SpaceInvadersGame::onResize(GameContext& ctx, int w, int h) { (void)ctx; (void)w; (void)h; }

void SpaceInvadersGame::resetGame() {
    player = Player();
    for (auto& b : bullets) b.active = false;
    particles.clear();

    enemyDirection = 1.0f;
    enemyMoveDown = false;
    enemyMoveTimer = 0;
    enemyShootTimer = 0;
    enemySpeed = ENEMY_BASE_SPEED + (wave - 1) * 0.03f;

    score = 0;
    lives = 3;
    gameOver = false;
    victory = false;
    paused = false;

    initEnemies();
}

void SpaceInvadersGame::initEnemies() {
    for (int row = 0; row < ENEMY_ROWS; ++row) {
        for (int col = 0; col < ENEMY_COLS; ++col) {
            Enemy& e = enemies[row * ENEMY_COLS + col];
            e.pos = Vec2(ENEMY_START_X + col * ENEMY_SPACING_X,
                         ENEMY_START_Y - row * ENEMY_SPACING_Y);
            e.alive = true;
            e.type = row < 1 ? 0 : (row < 3 ? 1 : 2);
            e.size = Vec2(0.08f, 0.06f);
            e.animTimer = (row * ENEMY_COLS + col) * 0.15f;
        }
    }
}

void SpaceInvadersGame::update(GameContext& ctx, float dt) {
    if (ctx.input->isKeyPressed(GLFW_KEY_R) && (gameOver || victory)) {
        wave = 1;
        resetGame();
        return;
    }
    if (ctx.input->isKeyPressed(GLFW_KEY_N) && victory) {
        wave++;
        resetGame();
        return;
    }
    if (paused || gameOver || victory) return;

    // Player movement
    if (ctx.input->isKeyDown(GLFW_KEY_LEFT) || ctx.input->isKeyDown(GLFW_KEY_A)) {
        player.pos.x -= PLAYER_SPEED * dt;
    }
    if (ctx.input->isKeyDown(GLFW_KEY_RIGHT) || ctx.input->isKeyDown(GLFW_KEY_D)) {
        player.pos.x += PLAYER_SPEED * dt;
    }
    float halfW = player.size.x * 0.5f;
    player.pos.x = std::max(-1.0f + halfW, std::min(1.0f - halfW, player.pos.x));

    // Player shooting
    player.shootCooldown = std::max(0.0f, player.shootCooldown - dt);
    if ((ctx.input->isKeyDown(GLFW_KEY_SPACE) || ctx.input->isKeyDown(GLFW_KEY_UP) || ctx.input->isKeyDown(GLFW_KEY_W))
        && player.shootCooldown <= 0) {
        spawnPlayerBullet();
        player.shootCooldown = SHOOT_COOLDOWN;
    }

    updateBullets(dt);
    updateEnemies(dt);
    updateParticles(dt);
    checkCollisions();

    if (allEnemiesDead()) victory = true;
}

void SpaceInvadersGame::updateBullets(float dt) {
    for (auto& b : bullets) {
        if (!b.active) continue;
        b.pos += b.vel * dt;
        if (b.pos.y > 1.1f || b.pos.y < -1.1f) b.active = false;
    }
}

void SpaceInvadersGame::updateEnemies(float dt) {
    enemyMoveTimer += dt;
    enemyShootTimer += dt;

    // Find bounds of alive enemies
    float leftmost = 10.0f, rightmost = -10.0f, bottommost = 10.0f;
    int aliveCount = 0;
    for (const auto& e : enemies) {
        if (!e.alive) continue;
        aliveCount++;
        float hw = e.size.x * 0.5f;
        leftmost = std::min(leftmost, e.pos.x - hw);
        rightmost = std::max(rightmost, e.pos.x + hw);
        bottommost = std::min(bottommost, e.pos.y - e.size.y * 0.5f);
    }
    if (aliveCount == 0) return;

    // Bounce at edges
    if ((rightmost >= 0.92f && enemyDirection > 0) || (leftmost <= -0.92f && enemyDirection < 0)) {
        enemyMoveDown = true;
        enemyDirection *= -1.0f;
    }

    // Grid steps (classic Space Invaders movement - discrete steps)
    float moveInterval = 0.5f / (1.0f + aliveCount * 0.03f);
    if (enemyMoveTimer >= moveInterval) {
        enemyMoveTimer = 0;
        for (auto& e : enemies) {
            if (!e.alive) continue;
            if (enemyMoveDown) {
                e.pos.y -= ENEMY_DROP;
            } else {
                e.pos.x += enemyDirection * enemySpeed;
            }
            e.animTimer += moveInterval;
        }
        enemyMoveDown = false;
    }

    // Enemy shooting
    if (enemyShootTimer >= ENEMY_SHOOT_INTERVAL) {
        enemyShootTimer = 0;
        spawnEnemyBullet();
    }

    // Enemies reached player
    if (bottommost <= player.pos.y + player.size.y * 0.5f) {
        gameOver = true;
    }
}

void SpaceInvadersGame::spawnPlayerBullet() {
    for (auto& b : bullets) {
        if (!b.active) {
            b.pos = player.pos + Vec2(0, player.size.y * 0.5f + 0.03f);
            b.vel = Vec2(0, BULLET_SPEED);
            b.size = Vec2(0.02f, 0.1f);
            b.active = true;
            b.isPlayerBullet = true;
            return;
        }
    }
}

void SpaceInvadersGame::spawnEnemyBullet() {
    // Pick a random alive enemy from the lowest alive in each column
    std::vector<Enemy*> shooters;
    for (auto& e : enemies) if (e.alive) shooters.push_back(&e);
    if (shooters.empty()) return;

    Enemy* shooter = shooters[rng() % shooters.size()];
    for (auto& b : bullets) {
        if (!b.active) {
            b.pos = shooter->pos + Vec2(0, -shooter->size.y * 0.5f - 0.03f);
            b.vel = Vec2(0, -BULLET_SPEED * 0.6f);
            b.size = Vec2(0.025f, 0.1f);
            b.active = true;
            b.isPlayerBullet = false;
            return;
        }
    }
}

void SpaceInvadersGame::updateParticles(float dt) {
    for (auto it = particles.begin(); it != particles.end();) {
        it->pos += it->vel * dt;
        it->vel.y -= 1.0f * dt; // gravity
        it->life -= dt;
        it->color.a = std::max(0.0f, it->life / it->maxLife);
        if (it->life <= 0) {
            it = particles.erase(it);
        } else {
            ++it;
        }
    }
}

void SpaceInvadersGame::spawnExplosion(const Vec2& pos, const glm::vec4& color, int count) {
    std::uniform_real_distribution<float> angDist(0, 2.0f * 3.14159f);
    std::uniform_real_distribution<float> speedDist(0.3f, 1.5f);
    std::uniform_real_distribution<float> lifeDist(0.3f, 0.8f);
    std::uniform_real_distribution<float> sizeDist(0.01f, 0.035f);

    for (int i = 0; i < count; ++i) {
        if (particles.size() >= 200) break;
        float angle = angDist(rng);
        float speed = speedDist(rng);
        Particle p;
        p.pos = pos;
        p.vel = Vec2(std::cos(angle) * speed, std::sin(angle) * speed);
        p.size = Vec2(sizeDist(rng), sizeDist(rng));
        p.color = color;
        p.maxLife = lifeDist(rng);
        p.life = p.maxLife;
        particles.push_back(p);
    }
}

void SpaceInvadersGame::checkCollisions() {
    for (auto& b : bullets) {
        if (!b.active) continue;

        if (b.isPlayerBullet) {
            for (auto& e : enemies) {
                if (!e.alive) continue;
                if (b.getRect().intersects(e.getRect())) {
                    b.active = false;
                    e.alive = false;
                    score += (e.type + 1) * 10;
                    glm::vec4 color = e.type == 0 ? glm::vec4(1, 0.2f, 0.2f, 1)
                                    : e.type == 1 ? glm::vec4(1, 0.6f, 0, 1)
                                                  : glm::vec4(1, 1, 0.2f, 1);
                    spawnExplosion(e.pos, color, 12);
                    break;
                }
            }
        } else {
            if (player.alive && b.getRect().intersects(player.getRect())) {
                b.active = false;
                lives--;
                spawnExplosion(player.pos, glm::vec4(0, 1, 0.3f, 1), 20);
                if (lives <= 0) {
                    player.alive = false;
                    gameOver = true;
                } else {
                    player.pos.x = 0;
                }
            }
        }
    }
}

bool SpaceInvadersGame::allEnemiesDead() {
    for (const auto& e : enemies) if (e.alive) return false;
    return true;
}

void SpaceInvadersGame::render(GameContext& ctx) {
    Renderer* r = ctx.renderer;
    r->beginFrame();

    if (!gameOver && !victory) {
        drawGame(r);
    }
    drawUI(r);

    r->endFrame();
}

void SpaceInvadersGame::drawGame(Renderer* r) {
    // Player ship - green triangle-ish shape (made of rects)
    if (player.alive) {
        float px = player.pos.x, py = player.pos.y;
        // Body
        r->drawRect(glm::vec2(px - 0.06f, py - 0.025f), glm::vec2(0.12f, 0.05f),
                    glm::vec4(0.1f, 0.9f, 0.2f, 1.0f));
        // Top nub
        r->drawRect(glm::vec2(px - 0.015f, py + 0.025f), glm::vec2(0.03f, 0.03f),
                    glm::vec4(0.1f, 0.9f, 0.2f, 1.0f));
        // Cockpit
        r->drawRect(glm::vec2(px - 0.01f, py - 0.01f), glm::vec2(0.02f, 0.03f),
                    glm::vec4(0.4f, 1.0f, 0.5f, 1.0f));
    }

    // Bullets
    for (const auto& b : bullets) {
        if (!b.active) continue;
        glm::vec4 color = b.isPlayerBullet ? glm::vec4(1, 1, 0.2f, 1)
                                           : glm::vec4(1, 0.3f, 0.3f, 1);
        r->drawRect(glm::vec2(b.pos.x - b.size.x * 0.5f, b.pos.y - b.size.y * 0.5f),
                    glm::vec2(b.size.x, b.size.y), color);
    }

    // Enemies - different shapes per type
    for (const auto& e : enemies) {
        if (!e.alive) continue;

        glm::vec4 bodyColor = e.type == 0 ? glm::vec4(1.0f, 0.2f, 0.2f, 1.0f)
                             : e.type == 1 ? glm::vec4(1.0f, 0.6f, 0.0f, 1.0f)
                                           : glm::vec4(1.0f, 1.0f, 0.2f, 1.0f);

        float ex = e.pos.x, ey = e.pos.y;
        float ew = e.size.x, eh = e.size.y;

        // Main body
        r->drawRect(glm::vec2(ex - ew * 0.5f, ey - eh * 0.5f), glm::vec2(ew, eh), bodyColor);

        // Eyes (black rects)
        float eyeW = 0.015f, eyeH = 0.02f;
        r->drawRect(glm::vec2(ex - 0.025f, ey + 0.005f), glm::vec2(eyeW, eyeH),
                    glm::vec4(0, 0, 0, 1));
        r->drawRect(glm::vec2(ex + 0.01f, ey + 0.005f), glm::vec2(eyeW, eyeH),
                    glm::vec4(0, 0, 0, 1));

        // Antennae (type-dependent)
        if (e.type == 0) {
            r->drawRect(glm::vec2(ex - 0.03f, ey + eh * 0.5f), glm::vec2(0.01f, 0.025f), bodyColor);
            r->drawRect(glm::vec2(ex + 0.02f, ey + eh * 0.5f), glm::vec2(0.01f, 0.025f), bodyColor);
        }

        // Legs - alternate animation
        float legPhase = std::sin(e.animTimer * 6.0f) * 0.01f;
        r->drawRect(glm::vec2(ex - 0.035f + legPhase, ey - eh * 0.5f - 0.015f),
                    glm::vec2(0.02f, 0.015f), bodyColor);
        r->drawRect(glm::vec2(ex + 0.015f - legPhase, ey - eh * 0.5f - 0.015f),
                    glm::vec2(0.02f, 0.015f), bodyColor);
    }

    // Particles
    for (const auto& p : particles) {
        r->drawRect(glm::vec2(p.pos.x - p.size.x * 0.5f, p.pos.y - p.size.y * 0.5f),
                    glm::vec2(p.size.x, p.size.y), p.color);
    }

    // Ground line
    r->drawRect(glm::vec2(-1.0f, -0.92f), glm::vec2(2.0f, 0.005f),
                glm::vec4(0.3f, 0.8f, 0.3f, 1.0f));
}

void SpaceInvadersGame::drawUI(Renderer* r) {
    // Lives label + indicator (top-left)
    r->drawText(glm::vec2(-0.97f, 0.87f), "LIVES", 0.04f, glm::vec4(0.3f, 1, 0.4f, 1));
    for (int i = 0; i < lives; ++i) {
        float x = -0.97f + i * 0.07f;
        float y = 0.78f;
        r->drawRect(glm::vec2(x, y), glm::vec2(0.05f, 0.025f),
                    glm::vec4(0.1f, 0.9f, 0.2f, 1.0f));
        r->drawRect(glm::vec2(x + 0.0175f, y + 0.025f), glm::vec2(0.015f, 0.015f),
                    glm::vec4(0.1f, 0.9f, 0.2f, 1.0f));
    }

    // Score (top-right)
    char scoreBuf[32];
    snprintf(scoreBuf, sizeof(scoreBuf), "SCORE %d", score);
    r->drawText(glm::vec2(0.45f, 0.87f), scoreBuf, 0.05f, glm::vec4(1, 1, 0.5f, 1));

    // Wave (top-center)
    char waveBuf[32];
    snprintf(waveBuf, sizeof(waveBuf), "WAVE %d", wave);
    r->drawText(glm::vec2(-0.12f, 0.87f), waveBuf, 0.05f, glm::vec4(0.6f, 0.7f, 1, 1));

    // Game Over overlay
    if (gameOver) {
        r->drawRect(glm::vec2(-1.0f, -1.0f), glm::vec2(2.0f, 2.0f),
                    glm::vec4(0.3f, 0.0f, 0.0f, 0.75f));
        r->drawText(glm::vec2(-0.28f, 0.05f), "GAME OVER", 0.1f,
                    glm::vec4(1, 0.2f, 0.2f, 1));
        char sbuf[32];
        snprintf(sbuf, sizeof(sbuf), "SCORE %d", score);
        r->drawText(glm::vec2(-0.15f, -0.1f), sbuf, 0.06f, glm::vec4(1, 1, 1, 1));
        r->drawText(glm::vec2(-0.3f, -0.3f), "PRESS R TO RESTART", 0.05f,
                    glm::vec4(0.8f, 0.8f, 0.8f, 1));
        r->drawText(glm::vec2(-0.3f, -0.4f), "ESC = MENU", 0.05f,
                    glm::vec4(0.6f, 0.6f, 0.7f, 1));
    }

    // Victory overlay
    if (victory) {
        r->drawRect(glm::vec2(-1.0f, -1.0f), glm::vec2(2.0f, 2.0f),
                    glm::vec4(0.0f, 0.2f, 0.0f, 0.75f));
        r->drawText(glm::vec2(-0.25f, 0.05f), "YOU WIN!", 0.1f,
                    glm::vec4(0.2f, 1, 0.4f, 1));
        char sbuf[32];
        snprintf(sbuf, sizeof(sbuf), "SCORE %d", score);
        r->drawText(glm::vec2(-0.15f, -0.1f), sbuf, 0.06f, glm::vec4(1, 1, 1, 1));
        r->drawText(glm::vec2(-0.3f, -0.3f), "N = NEXT WAVE", 0.05f,
                    glm::vec4(0.8f, 0.8f, 0.8f, 1));
        r->drawText(glm::vec2(-0.3f, -0.4f), "R = RESTART   ESC = MENU", 0.05f,
                    glm::vec4(0.6f, 0.6f, 0.7f, 1));
    }
}
