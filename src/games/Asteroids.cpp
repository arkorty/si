#include "Asteroids.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

constexpr float SHIP_ACCEL = 1.5f;
constexpr float SHIP_MAX_SPEED = 1.0f;
constexpr float SHIP_FRICTION = 0.99f;
constexpr float SHIP_TURN_SPEED = 4.0f;
constexpr float BULLET_SPEED = 1.8f;
constexpr float BULLET_LIFE = 1.2f;
constexpr float SHOOT_COOLDOWN = 0.3f;
constexpr float WRAP_MARGIN = 0.05f;

static float randf(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

// Scanline-fill a triangle with axis-aligned rects (verts in world space)
static void fillTriangle(Renderer* r, AVec2 a, AVec2 b, AVec2 c,
                         const glm::vec4& color) {
    const float step = 0.0035f;
    float minX = std::min(a.x, std::min(b.x, c.x));
    float maxX = std::max(a.x, std::max(b.x, c.x));
    float minY = std::min(a.y, std::min(b.y, c.y));
    float maxY = std::max(a.y, std::max(b.y, c.y));
    if (maxX - minX < 1e-6f || maxY - minY < 1e-6f) return;

    for (float y = minY; y <= maxY; y += step) {
        float xs[3];
        int n = 0;
        auto edge = [&](const AVec2& p, const AVec2& q) {
            // half-open rule avoids double-hit at shared vertices
            if ((p.y <= y && q.y > y) || (q.y <= y && p.y > y)) {
                float t = (y - p.y) / (q.y - p.y);
                if (n < 3) xs[n++] = p.x + t * (q.x - p.x);
            }
        };
        edge(a, b);
        edge(b, c);
        edge(c, a);
        if (n < 2) continue;
        float x0 = xs[0], x1 = xs[0];
        for (int i = 1; i < n; ++i) {
            x0 = std::min(x0, xs[i]);
            x1 = std::max(x1, xs[i]);
        }
        if (x1 - x0 > 1e-6f) {
            r->drawRect(glm::vec2(x0, y), glm::vec2(x1 - x0, step + 0.001f), color);
        }
    }
}

AsteroidsGame::AsteroidsGame()
    : shipPos(0, 0), shipVel(0, 0), shipAngle(0), shipAlive(true), thrusting(false),
      flamePhase(0), shipInvincible(0), shootCooldown(0), score(0), lives(3), level(1),
      gameOver(false), paused(false), respawnTimer(0),
      rng(std::random_device{}()) {
    bullets.resize(30); // resize, not reserve — need actual elements to reuse
    asteroids.reserve(30);
    particles.reserve(200);
}

void AsteroidsGame::init(GameContext& ctx) { (void)ctx; resetGame(); }
void AsteroidsGame::cleanup(GameContext& ctx) { (void)ctx; }
void AsteroidsGame::onEnter(GameContext& ctx) { (void)ctx; resetGame(); }
void AsteroidsGame::onExit(GameContext& ctx) { (void)ctx; }
void AsteroidsGame::onKey(GameContext& ctx, int k, int a) { (void)ctx; (void)k; (void)a; }
void AsteroidsGame::onResize(GameContext& ctx, int w, int h) { (void)ctx; (void)w; (void)h; }

void AsteroidsGame::resetGame() {
    shipPos = AVec2(0, 0);
    shipVel = AVec2(0, 0);
    shipAngle = 3.14159f / 2.0f; // point up
    shipAlive = true;
    thrusting = false;
    flamePhase = 0;
    shipInvincible = 2.0f;
    shootCooldown = 0;
    respawnTimer = 0;

    for (auto& b : bullets) b.active = false;
    asteroids.clear();
    particles.clear();

    score = 0;
    lives = 3;
    level = 1;
    gameOver = false;
    paused = false;

    spawnLargeAsteroids();
}

void AsteroidsGame::spawnLargeAsteroids() {
    int count = 3 + level;
    for (int i = 0; i < count; ++i) {
        // Spawn away from center
        float angle = randf(rng, 0, 6.28318f);
        float dist = randf(rng, 0.5f, 0.9f);
        AVec2 pos(std::cos(angle) * dist, std::sin(angle) * dist);
        spawnAsteroid(3, pos);
    }
}

void AsteroidsGame::spawnAsteroid(int size, AVec2 pos) {
    Asteroid a;
    a.pos = pos;
    a.size = size;
    a.radius = size == 3 ? 0.1f : size == 2 ? 0.055f : 0.03f;
    a.alive = true;
    a.rotation = randf(rng, 0, 6.28318f);
    a.rotSpeed = randf(rng, -1.5f, 1.5f);

    float speed = (4 - size) * 0.15f + 0.1f;
    float dir = randf(rng, 0, 6.28318f);
    a.vel = AVec2(std::cos(dir) * speed, std::sin(dir) * speed);

    // Generate irregular polygon shape
    int verts = 8 + (rng() % 5);
    a.shape.clear();
    for (int i = 0; i < verts; ++i) {
        float t = (float)i / verts * 6.28318f;
        float r = a.radius * randf(rng, 0.7f, 1.3f);
        a.shape.push_back(AVec2(std::cos(t) * r, std::sin(t) * r));
    }

    asteroids.push_back(a);
}

void AsteroidsGame::wrap(AVec2& p) {
    if (p.x > 1.0f + WRAP_MARGIN) p.x = -1.0f - WRAP_MARGIN;
    if (p.x < -1.0f - WRAP_MARGIN) p.x = 1.0f + WRAP_MARGIN;
    if (p.y > 1.0f + WRAP_MARGIN) p.y = -1.0f - WRAP_MARGIN;
    if (p.y < -1.0f - WRAP_MARGIN) p.y = 1.0f + WRAP_MARGIN;
}

void AsteroidsGame::update(GameContext& ctx, float dt) {
    if (ctx.input->isKeyPressed(GLFW_KEY_R) && gameOver) {
        level = 1;
        resetGame();
        return;
    }
    if (ctx.input->isKeyPressed(GLFW_KEY_P)) paused = !paused;
    if (paused || gameOver) return;

    shootCooldown = std::max(0.0f, shootCooldown - dt);
    shipInvincible = std::max(0.0f, shipInvincible - dt);
    flamePhase += dt * 30.0f;

    if (shipAlive) updatePlayer(dt, ctx);
    else {
        respawnTimer -= dt;
        if (respawnTimer <= 0) {
            shipAlive = true;
            shipPos = AVec2(0, 0);
            shipVel = AVec2(0, 0);
            shipAngle = 3.14159f / 2.0f;
            thrusting = false;
            shipInvincible = 2.0f;
        }
    }

    updateAsteroids(dt);
    updateBullets(dt);
    updateParticles(dt);
    checkCollisions();

    // Check level clear
    if (aliveAsteroids() == 0) {
        level++;
        spawnLargeAsteroids();
        shipInvincible = 2.0f;
    }
}

void AsteroidsGame::updatePlayer(float dt, GameContext& ctx) {
    // Rotation
    if (ctx.input->isKeyDown(GLFW_KEY_LEFT) || ctx.input->isKeyDown(GLFW_KEY_A)) {
        shipAngle -= SHIP_TURN_SPEED * dt;
    }
    if (ctx.input->isKeyDown(GLFW_KEY_RIGHT) || ctx.input->isKeyDown(GLFW_KEY_D)) {
        shipAngle += SHIP_TURN_SPEED * dt;
    }

    // Thrust
    thrusting = ctx.input->isKeyDown(GLFW_KEY_UP) || ctx.input->isKeyDown(GLFW_KEY_W);
    if (thrusting) {
        shipVel.x += std::cos(shipAngle) * SHIP_ACCEL * dt;
        shipVel.y += std::sin(shipAngle) * SHIP_ACCEL * dt;
    }

    // Friction + clamp speed
    shipVel = shipVel * SHIP_FRICTION;
    float speed2 = shipVel.length();
    if (speed2 > SHIP_MAX_SPEED * SHIP_MAX_SPEED) {
        float s = std::sqrt(speed2);
        shipVel = shipVel * (SHIP_MAX_SPEED / s);
    }

    shipPos += shipVel * dt;
    wrap(shipPos);

    // Shooting
    if ((ctx.input->isKeyDown(GLFW_KEY_SPACE)) && shootCooldown <= 0) {
        for (auto& b : bullets) {
            if (!b.active) {
                b.pos = shipPos + AVec2(std::cos(shipAngle), std::sin(shipAngle)) * 0.04f;
                b.vel = AVec2(std::cos(shipAngle), std::sin(shipAngle)) * BULLET_SPEED + shipVel;
                b.active = true;
                b.life = BULLET_LIFE;
                shootCooldown = SHOOT_COOLDOWN;
                break;
            }
        }
    }
}

void AsteroidsGame::updateAsteroids(float dt) {
    for (auto& a : asteroids) {
        if (!a.alive) continue;
        a.pos += a.vel * dt;
        a.rotation += a.rotSpeed * dt;
        wrap(a.pos);
    }
}

void AsteroidsGame::updateBullets(float dt) {
    for (auto& b : bullets) {
        if (!b.active) continue;
        b.pos += b.vel * dt;
        b.life -= dt;
        if (b.life <= 0) { b.active = false; continue; }
        wrap(b.pos);
    }
}

void AsteroidsGame::updateParticles(float dt) {
    for (auto it = particles.begin(); it != particles.end();) {
        it->pos += it->vel * dt;
        it->life -= dt;
        it->color.a = std::max(0.0f, it->life / it->maxLife);
        if (it->life <= 0) it = particles.erase(it);
        else ++it;
    }
}

void AsteroidsGame::spawnExplosion(const AVec2& pos, const glm::vec4& color, int count) {
    for (int i = 0; i < count; ++i) {
        if (particles.size() >= 200) break;
        float angle = randf(rng, 0, 6.28318f);
        float speed = randf(rng, 0.2f, 1.0f);
        AsteroidParticle p;
        p.pos = pos;
        p.vel = AVec2(std::cos(angle) * speed, std::sin(angle) * speed);
        p.maxLife = randf(rng, 0.3f, 0.9f);
        p.life = p.maxLife;
        p.color = color;
        particles.push_back(p);
    }
}

void AsteroidsGame::splitAsteroid(Asteroid& a) {
    a.alive = false;
    score += (4 - a.size) * (4 - a.size) * 5;

    if (a.size > 1) {
        spawnAsteroid(a.size - 1, a.pos);
        spawnAsteroid(a.size - 1, a.pos);
    }

    glm::vec4 col = a.size == 3 ? glm::vec4(0.8f, 0.8f, 1, 1)
                  : a.size == 2 ? glm::vec4(0.6f, 0.7f, 1, 1)
                                : glm::vec4(0.5f, 0.6f, 1, 1);
    spawnExplosion(a.pos, col, 8 + a.size * 4);
}

void AsteroidsGame::checkCollisions() {
    // Bullets vs asteroids
    for (auto& b : bullets) {
        if (!b.active) continue;
        for (auto& a : asteroids) {
            if (!a.alive) continue;
            AVec2 d = b.pos - a.pos;
            if (d.length() < a.radius * a.radius) {
                b.active = false;
                splitAsteroid(a);
                break;
            }
        }
    }

    // Ship vs asteroids
    if (shipAlive && shipInvincible <= 0) {
        for (auto& a : asteroids) {
            if (!a.alive) continue;
            AVec2 d = shipPos - a.pos;
            float hitR = a.radius + 0.03f;
            if (d.length() < hitR * hitR) {
                shipAlive = false;
                lives--;
                respawnTimer = 2.0f;
                spawnExplosion(shipPos, glm::vec4(0.3f, 1, 0.5f, 1), 30);
                if (lives <= 0) gameOver = true;
                break;
            }
        }
    }
}

int AsteroidsGame::aliveAsteroids() {
    int n = 0;
    for (auto& a : asteroids) if (a.alive) n++;
    return n;
}

void AsteroidsGame::render(GameContext& ctx) {
    Renderer* r = ctx.renderer;

    if (!gameOver) {
        drawAsteroids(r);
        drawBullets(r);
        drawParticles(r);
        if (shipAlive) drawShip(r);
    }
    drawUI(r);
}

void AsteroidsGame::drawShip(Renderer* r) {
    if (shipInvincible > 0 && (int)(shipInvincible * 8) % 2 == 0) return; // blink

    float ca = std::cos(shipAngle);
    float sa = std::sin(shipAngle);
    float s = 0.055f;

    // Classic Asteroids hull (local space, nose along +X):
    //   nose
    //   /  \
    // wing  wing
    //  \   /
    //   notch  (concave rear)
    auto world = [&](float lx, float ly) {
        float rx = lx * ca - ly * sa;
        float ry = lx * sa + ly * ca;
        return AVec2(shipPos.x + rx * s, shipPos.y + ry * s);
    };

    AVec2 nose  = world( 1.00f,  0.00f);
    AVec2 rearL = world(-0.70f,  0.60f);
    AVec2 notch = world(-0.35f,  0.00f);
    AVec2 rearR = world(-0.70f, -0.60f);

    // Engine flame (behind ship, only while thrusting)
    if (thrusting) {
        float flicker = 0.75f + 0.25f * std::sin(flamePhase + shipAngle * 3.0f);
        float fl = s * (0.55f + 0.35f * flicker);
        AVec2 flL = world(-0.70f,  0.30f);
        AVec2 flR = world(-0.70f, -0.30f);
        AVec2 tip = world(-0.70f - fl / s, 0.0f);
        fillTriangle(r, flL, tip, flR, glm::vec4(1.0f, 0.55f, 0.1f, 0.95f));
        // hot inner core
        AVec2 inL = world(-0.70f,  0.16f);
        AVec2 inR = world(-0.70f, -0.16f);
        AVec2 inT = world(-0.70f - fl * 0.55f / s, 0.0f);
        fillTriangle(r, inL, inT, inR, glm::vec4(1.0f, 0.95f, 0.4f, 1.0f));
    }

    // Solid hull as two triangles sharing nose–notch (handles concave notch cleanly)
    const glm::vec4 fill(0.12f, 0.85f, 0.35f, 1.0f);
    fillTriangle(r, nose, rearL, notch, fill);
    fillTriangle(r, nose, notch, rearR, fill);
    // rear body between wings and notch
    fillTriangle(r, rearL, rearR, notch, fill);

    // Bright outline (slightly inset feathering via joint caps)
    const glm::vec4 outline(0.55f, 1.0f, 0.7f, 1.0f);
    auto drawEdge = [&](const AVec2& p1, const AVec2& p2) {
        AVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
        float dx = p2.x - p1.x, dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) return;
        float angle = std::atan2(dy, dx);
        r->drawRect(glm::vec2(mid.x - len * 0.5f, mid.y - 0.0025f),
                    glm::vec2(len, 0.005f), outline, angle);
    };
    drawEdge(nose, rearL);
    drawEdge(rearL, notch);
    drawEdge(notch, rearR);
    drawEdge(rearR, nose);

    // Joint caps so corners don't show gaps
    const float cap = 0.006f;
    auto capAt = [&](const AVec2& p) {
        r->drawRect(glm::vec2(p.x - cap * 0.5f, p.y - cap * 0.5f),
                    glm::vec2(cap, cap), outline);
    };
    capAt(nose);
    capAt(rearL);
    capAt(rearR);
    capAt(notch);

    // Cockpit glint
    AVec2 cockpit = world(0.15f, 0.0f);
    r->drawRect(glm::vec2(cockpit.x - 0.006f, cockpit.y - 0.006f),
                glm::vec2(0.012f, 0.012f), glm::vec4(0.85f, 1.0f, 0.9f, 1.0f));
}

void AsteroidsGame::drawAsteroids(Renderer* r) {
    for (auto& a : asteroids) {
        if (!a.alive) continue;

        glm::vec4 col = a.size == 3 ? glm::vec4(0.8f, 0.8f, 1, 1)
                      : a.size == 2 ? glm::vec4(0.6f, 0.7f, 1, 1)
                                    : glm::vec4(0.5f, 0.6f, 1, 1);

        float cr = std::cos(a.rotation);
        float sr = std::sin(a.rotation);

        // Draw each edge of the polygon
        for (size_t i = 0; i < a.shape.size(); ++i) {
            AVec2 p1 = a.shape[i];
            AVec2 p2 = a.shape[(i + 1) % a.shape.size()];

            // Rotate
            AVec2 rp1(a.pos.x + p1.x * cr - p1.y * sr, a.pos.y + p1.x * sr + p1.y * cr);
            AVec2 rp2(a.pos.x + p2.x * cr - p2.y * sr, a.pos.y + p2.x * sr + p2.y * cr);

            AVec2 mid((rp1.x + rp2.x) * 0.5f, (rp1.y + rp2.y) * 0.5f);
            float dx = rp2.x - rp1.x, dy = rp2.y - rp1.y;
            float len = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx);
            r->drawRect(glm::vec2(mid.x - len * 0.5f, mid.y - 0.002f),
                        glm::vec2(len, 0.004f), col, angle);
        }
    }
}

void AsteroidsGame::drawBullets(Renderer* r) {
    for (auto& b : bullets) {
        if (!b.active) continue;
        r->drawRect(glm::vec2(b.pos.x - 0.005f, b.pos.y - 0.005f),
                    glm::vec2(0.01f, 0.01f), glm::vec4(1, 1, 0.4f, 1));
    }
}

void AsteroidsGame::drawParticles(Renderer* r) {
    for (auto& p : particles) {
        r->drawRect(glm::vec2(p.pos.x - 0.005f, p.pos.y - 0.005f),
                    glm::vec2(0.01f, 0.01f), p.color);
    }
}

void AsteroidsGame::drawUI(Renderer* r) {
    char buf[64];

    snprintf(buf, sizeof(buf), "SCORE %d", score);
    r->drawText(glm::vec2(0.45f, 0.87f), buf, 0.05f, glm::vec4(1, 1, 0.5f, 1));

    snprintf(buf, sizeof(buf), "LEVEL %d", level);
    r->drawText(glm::vec2(-0.1f, 0.87f), buf, 0.05f, glm::vec4(0.6f, 0.7f, 1, 1));

    r->drawText(glm::vec2(-0.97f, 0.87f), "LIVES", 0.04f, glm::vec4(0.3f, 1, 0.4f, 1));
    for (int i = 0; i < lives; ++i) {
        // Draw small ship icons
        float x = -0.97f + i * 0.06f;
        r->drawRect(glm::vec2(x, 0.78f), glm::vec2(0.04f, 0.02f), glm::vec4(0.3f, 1, 0.5f, 1));
        r->drawRect(glm::vec2(x + 0.012f, 0.80f), glm::vec2(0.016f, 0.015f), glm::vec4(0.3f, 1, 0.5f, 1));
    }

    if (paused) {
        r->drawRect(glm::vec2(-0.2f, -0.05f), glm::vec2(0.4f, 0.1f), glm::vec4(0, 0, 0.3f, 0.85f));
        r->drawText(glm::vec2(-0.1f, -0.02f), "PAUSED", 0.06f, glm::vec4(1, 1, 1, 1));
    }

    if (gameOver) {
        r->drawRect(glm::vec2(-1, -1), glm::vec2(2, 2), glm::vec4(0.3f, 0, 0, 0.75f));
        r->drawText(glm::vec2(-0.28f, 0.05f), "GAME OVER", 0.1f, glm::vec4(1, 0.2f, 0.2f, 1));
        snprintf(buf, sizeof(buf), "SCORE %d", score);
        r->drawText(glm::vec2(-0.15f, -0.1f), buf, 0.06f, glm::vec4(1, 1, 1, 1));
        r->drawText(glm::vec2(-0.3f, -0.3f), "PRESS R TO RESTART", 0.05f, glm::vec4(0.8f, 0.8f, 0.8f, 1));
        r->drawText(glm::vec2(-0.3f, -0.4f), "ESC = MENU", 0.05f, glm::vec4(0.6f, 0.6f, 0.7f, 1));
    }
}
