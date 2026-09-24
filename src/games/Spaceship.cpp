#include "Spaceship.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

// ============================================================
// Constructor / Destructor
// ============================================================
SpaceshipGame::SpaceshipGame() {
    std::random_device rd;
    rng.seed(rd());
}

SpaceshipGame::~SpaceshipGame() {
    cleanup3DPipeline();
}

// ============================================================
// Random helper
// ============================================================
float SpaceshipGame::randF(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

// ============================================================
// 3D pipeline (self-contained – does not touch Renderer's GL state)
// ============================================================
GLuint SpaceshipGame::compile3DShader(const char* path, GLenum type) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Spaceship3D] Failed to open shader: " << path << "\n";
        return 0;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();
    const char* csrc = src.c_str();

    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &csrc, nullptr);
    glCompileShader(s);

    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "[Spaceship3D] Shader error (" << path << "): " << log << "\n";
        return 0;
    }
    return s;
}

void SpaceshipGame::init3DPipeline() {
    GLuint vert = compile3DShader("shaders/spaceship3d.vert", GL_VERTEX_SHADER);
    GLuint frag = compile3DShader("shaders/spaceship3d.frag", GL_FRAGMENT_SHADER);
    if (!vert || !frag) {
        std::cerr << "[Spaceship3D] Shader compilation failed!\n";
        return;
    }

    shader3D = glCreateProgram();
    glAttachShader(shader3D, vert);
    glAttachShader(shader3D, frag);
    glLinkProgram(shader3D);

    int ok;
    glGetProgramiv(shader3D, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(shader3D, 512, nullptr, log);
        std::cerr << "[Spaceship3D] Link error: " << log << "\n";
        shader3D = 0;
        return;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);

    locModel = glGetUniformLocation(shader3D, "model");
    locView  = glGetUniformLocation(shader3D, "view");
    locProj  = glGetUniformLocation(shader3D, "projection");
    locAlpha = glGetUniformLocation(shader3D, "alpha");

    glGenVertexArrays(1, &vao3D);
    glGenBuffers(1, &vbo3D);

    glBindVertexArray(vao3D);
    glBindBuffer(GL_ARRAY_BUFFER, vbo3D);
    // layout: pos(3) + color(3) = 6 floats per vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    pipelineReady = true;
}

void SpaceshipGame::cleanup3DPipeline() {
    if (shader3D) { glDeleteProgram(shader3D); shader3D = 0; }
    if (vao3D) { glDeleteVertexArrays(1, &vao3D); vao3D = 0; }
    if (vbo3D) { glDeleteBuffers(1, &vbo3D); vbo3D = 0; }
    pipelineReady = false;
}

void SpaceshipGame::beginFrame3D(int w, int h) {
    // Clear to black for space
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glLineWidth(1.0f);

    projMatrix = Mat4::perspective(60.0f * 3.14159f / 180.0f, (float)w / h, 0.1f, 2000.0f);
}

void SpaceshipGame::drawMesh(const Mesh& mesh, const Mat4& model, float alpha) {
    if (!pipelineReady || mesh.vertices.empty()) return;

    // Pack interleaved: pos(3) + color(3)
    std::vector<float> data;
    data.reserve(mesh.vertices.size() * 6);
    for (const auto& v : mesh.vertices) {
        data.push_back(v.pos.x);
        data.push_back(v.pos.y);
        data.push_back(v.pos.z);
        data.push_back(v.color.x);
        data.push_back(v.color.y);
        data.push_back(v.color.z);
    }

    glUseProgram(shader3D);
    glUniformMatrix4fv(locModel, 1, GL_FALSE, model.data());
    glUniformMatrix4fv(locView, 1, GL_FALSE, viewMatrix.data());
    glUniformMatrix4fv(locProj, 1, GL_FALSE, projMatrix.data());
    glUniform1f(locAlpha, alpha);

    glBindVertexArray(vao3D);
    glBindBuffer(GL_ARRAY_BUFFER, vbo3D);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(mesh.vertices.size()));
    glBindVertexArray(0);
}

// ============================================================
// Mesh helpers
// ============================================================
void SpaceshipGame::pushTri(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 col) {
    m.vertices.push_back({a, col});
    m.vertices.push_back({b, col});
    m.vertices.push_back({c, col});
}

void SpaceshipGame::pushTri(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 colA, Vec3 colB, Vec3 colC) {
    m.vertices.push_back({a, colA});
    m.vertices.push_back({b, colB});
    m.vertices.push_back({c, colC});
}

// ============================================================
// Mesh generators — retro triangle style
// ============================================================
SpaceshipGame::Mesh SpaceshipGame::makeShipMesh() {
    // Wedge spaceship pointing along +Z, centered at origin
    Mesh m;
    Vec3 cBody(0.0f, 0.9f, 0.4f);   // teal green body
    Vec3 cDark(0.0f, 0.5f, 0.25f);  // darker shade
    Vec3 cEdge(0.0f, 1.0f, 0.6f);   // bright edge
    Vec3 cCockpit(0.2f, 0.6f, 1.0f); // blue cockpit
    Vec3 cEngine(1.0f, 0.4f, 0.0f);  // orange engine

    // Key vertices
    Vec3 nose(0, 0, 2.0f);
    Vec3 rearTL(-1.0f,  0.15f, -1.2f);
    Vec3 rearTR( 1.0f,  0.15f, -1.2f);
    Vec3 rearBL(-1.0f, -0.15f, -1.2f);
    Vec3 rearBR( 1.0f, -0.15f, -1.2f);
    Vec3 midTL(-0.5f,  0.2f, 0.5f);
    Vec3 midTR( 0.5f,  0.2f, 0.5f);
    Vec3 midBL(-0.5f, -0.1f, 0.5f);
    Vec3 midBR( 0.5f, -0.1f, 0.5f);
    Vec3 wingL(-1.8f, 0.0f, -0.8f);
    Vec3 wingR( 1.8f, 0.0f, -0.8f);

    // Top surface
    pushTri(m, nose, midTL, midTR, cEdge, cBody, cBody);
    pushTri(m, midTL, rearTL, rearTR, cBody, cDark, cDark);
    pushTri(m, midTL, rearTR, midTR, cBody, cDark, cBody);

    // Bottom surface
    pushTri(m, nose, midBR, midBL, cDark, cDark, cDark);
    pushTri(m, midBL, midBR, rearBR, cDark, cDark, cDark);
    pushTri(m, midBL, rearBR, rearBL, cDark, cDark, cDark);

    // Left side
    pushTri(m, nose, midBL, midTL, cBody, cDark, cBody);
    pushTri(m, midTL, midBL, rearBL, cBody, cDark, cDark);
    pushTri(m, midTL, rearBL, rearTL, cBody, cDark, cDark);

    // Right side
    pushTri(m, nose, midTR, midBR, cBody, cBody, cDark);
    pushTri(m, midTR, rearBR, midBR, cBody, cDark, cDark);
    pushTri(m, midTR, rearTR, rearBR, cBody, cDark, cDark);

    // Rear face
    pushTri(m, rearTL, rearBL, rearBR, cEngine, cEngine, cEngine);
    pushTri(m, rearTL, rearBR, rearTR, cEngine, cEngine, cEngine);

    // Wings (left)
    pushTri(m, midTL, wingL, rearTL, cEdge, cBody, cDark);
    pushTri(m, midBL, rearBL, wingL, cDark, cDark, cDark);
    pushTri(m, midTL, midBL, wingL, cBody, cDark, cBody);

    // Wings (right)
    pushTri(m, midTR, rearTR, wingR, cEdge, cDark, cBody);
    pushTri(m, midBR, wingR, rearBR, cDark, cDark, cDark);
    pushTri(m, midTR, wingR, midBR, cBody, cBody, cDark);

    // Cockpit ridge
    Vec3 cockpitPeak(0, 0.35f, 0.8f);
    pushTri(m, nose, cockpitPeak, midTL, cCockpit, cCockpit, cBody);
    pushTri(m, nose, midTR, cockpitPeak, cCockpit, cBody, cCockpit);
    pushTri(m, cockpitPeak, midTR, midTL, cCockpit, cBody, cBody);

    return m;
}

SpaceshipGame::Mesh SpaceshipGame::makeAsteroidMesh(float radius, int detail) {
    Mesh m;
    int slices = detail;
    int stacks = detail;

    // Build grid of displaced sphere points
    std::vector<Vec3> pts;
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * 3.14159f * j / slices;
            float displacement = radius * (0.6f + randF(0.0f, 0.5f));
            Vec3 p(
                displacement * sinf(phi) * cosf(theta),
                displacement * cosf(phi),
                displacement * sinf(phi) * sinf(theta)
            );
            pts.push_back(p);
        }
    }

    Vec3 colA(0.6f, 0.55f, 0.5f);
    Vec3 colB(0.5f, 0.45f, 0.4f);
    Vec3 colC(0.7f, 0.65f, 0.55f);

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            int a = i * (slices + 1) + j;
            int b = a + slices + 1;
            int c = a + 1;
            int d = b + 1;

            Vec3 c1 = (i + j) % 3 == 0 ? colA : ((i + j) % 3 == 1 ? colB : colC);
            Vec3 c2 = (i + j + 1) % 3 == 0 ? colA : ((i + j + 1) % 3 == 1 ? colB : colC);

            pushTri(m, pts[a], pts[b], pts[c], c1);
            pushTri(m, pts[c], pts[b], pts[d], c2);
        }
    }

    return m;
}

SpaceshipGame::Mesh SpaceshipGame::makeBulletMesh() {
    Mesh m;
    Vec3 col(1.0f, 1.0f, 0.2f);
    Vec3 colTip(1.0f, 0.6f, 0.0f);

    float len = 0.5f;
    float w = 0.1f;

    Vec3 front(0, 0, len);
    Vec3 back(0, 0, -len * 0.5f);
    Vec3 top(0, w, 0);
    Vec3 bot(0, -w, 0);
    Vec3 left(-w, 0, 0);
    Vec3 right(w, 0, 0);

    // Front 4 faces
    pushTri(m, front, top, right, colTip, col, col);
    pushTri(m, front, right, bot, colTip, col, col);
    pushTri(m, front, bot, left, colTip, col, col);
    pushTri(m, front, left, top, colTip, col, col);

    // Back 4 faces
    pushTri(m, back, right, top, col);
    pushTri(m, back, bot, right, col);
    pushTri(m, back, left, bot, col);
    pushTri(m, back, top, left, col);

    return m;
}

// ============================================================
// IGame interface
// ============================================================
void SpaceshipGame::init(GameContext& ctx) {
    (void)ctx;
    init3DPipeline();
    shipMesh = makeShipMesh();
    bulletMesh = makeBulletMesh();
}

void SpaceshipGame::cleanup(GameContext& ctx) {
    (void)ctx;
    cleanup3DPipeline();
}

void SpaceshipGame::onEnter(GameContext& ctx) {
    (void)ctx;
    resetGame();
}

void SpaceshipGame::onExit(GameContext& ctx) {
    (void)ctx;
}

void SpaceshipGame::onResize(GameContext& ctx, int width, int height) {
    (void)ctx; (void)width; (void)height;
}

void SpaceshipGame::onKey(GameContext& ctx, int key, int action) {
    (void)ctx;
    if (action == GLFW_PRESS && key == GLFW_KEY_R && gameOver) {
        resetGame();
    }
}

// ============================================================
// Reset
// ============================================================
void SpaceshipGame::resetGame() {
    ship.pos = Vec3(0, 0, 0);
    ship.vel = Vec3(0, 0, 0);
    ship.yaw = 0;
    ship.pitch = 0;
    ship.roll = 0;
    ship.forwardSpeed = 0;
    ship.baseSpeed = 15.0f;
    ship.speedIncrease = 0;

    cameraYaw = 0;
    cameraPitch = 0;

    bullets.clear();
    asteroids.clear();
    particles.clear();
    stars.clear();
    shootCooldown = 0;

    score = 0;
    lives = 3;
    gameOver = false;
    invincibleTimer = 2.0f;
    spawnTimer = 0;
    difficulty = 1.0f;
    totalTime = 0;

    // Create star field
    for (int i = 0; i < 500; i++) {
        Star s;
        s.pos = Vec3(randF(-500, 500), randF(-500, 500), randF(-500, 500));
        s.brightness = randF(0.3f, 1.0f);
        stars.push_back(s);
    }

    // Spawn initial asteroids
    for (int i = 0; i < 6; i++) {
        spawnAsteroid();
    }
}

// ============================================================
// Update
// ============================================================
void SpaceshipGame::update(GameContext& ctx, float dt) {
    if (gameOver) return;

    totalTime += dt;
    shootCooldown -= dt;
    if (invincibleTimer > 0) invincibleTimer -= dt;

    // Increase speed over time
    ship.speedIncrease = totalTime * 1.5f;
    ship.forwardSpeed = ship.baseSpeed + ship.speedIncrease;
    difficulty = 1.0f + totalTime / 20.0f;

    // ---- Ship steering ----
    float turnRate = 2.5f;
    float pitchRate = 2.0f;

    bool inputLeft  = ctx.input->isKeyDown(GLFW_KEY_LEFT)  || ctx.input->isKeyDown(GLFW_KEY_A);
    bool inputRight = ctx.input->isKeyDown(GLFW_KEY_RIGHT) || ctx.input->isKeyDown(GLFW_KEY_D);
    bool inputUp    = ctx.input->isKeyDown(GLFW_KEY_UP)    || ctx.input->isKeyDown(GLFW_KEY_W);
    bool inputDown  = ctx.input->isKeyDown(GLFW_KEY_DOWN)  || ctx.input->isKeyDown(GLFW_KEY_S);
    bool inputShoot = ctx.input->isKeyDown(GLFW_KEY_SPACE);

    if (inputLeft)  ship.yaw += turnRate * dt;
    if (inputRight) ship.yaw -= turnRate * dt;
    if (inputUp)    ship.pitch -= pitchRate * dt;
    if (inputDown)  ship.pitch += pitchRate * dt;

    // Clamp pitch
    float maxPitch = 1.2f;
    if (ship.pitch >  maxPitch) ship.pitch =  maxPitch;
    if (ship.pitch < -maxPitch) ship.pitch = -maxPitch;

    // Visual roll
    float targetRoll = 0;
    if (inputLeft)  targetRoll =  0.5f;
    if (inputRight) targetRoll = -0.5f;
    ship.roll += (targetRoll - ship.roll) * 5.0f * dt;

    // Smoothly interpolate camera to follow ship's rotation
    cameraYaw += (ship.yaw - cameraYaw) * 4.0f * dt;
    cameraPitch += (ship.pitch - cameraPitch) * 4.0f * dt;

    // Forward direction from yaw + pitch (exactly matches mesh rotation)
    Vec3 forward(
         sinf(ship.yaw) * cosf(ship.pitch),
        -sinf(ship.pitch),
         cosf(ship.yaw) * cosf(ship.pitch)
    );
    forward = forward.normalized();

    // Ship always moves forward
    ship.vel = forward * ship.forwardSpeed;
    ship.pos += ship.vel * dt;

    // ---- Shooting ----
    if (inputShoot && shootCooldown <= 0) {
        Bullet b;
        b.pos = ship.pos + forward * 3.0f;
        b.vel = forward * (ship.forwardSpeed + 120.0f);
        b.life = 2.0f;
        bullets.push_back(b);
        shootCooldown = 0.12f;
    }

    // ---- Spawn asteroids ----
    spawnTimer += dt;
    float spawnInterval = std::max(0.8f, 3.0f / difficulty);
    if (spawnTimer > spawnInterval) {
        int count = 1 + (int)(difficulty / 3.0f);
        for (int i = 0; i < count; i++) {
            spawnAsteroid();
        }
        spawnTimer = 0;
    }

    // ---- Update bullets ----
    for (auto& b : bullets) {
        b.pos += b.vel * dt;
        b.life -= dt;
    }
    Vec3 sp = ship.pos;
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [&sp](const Bullet& b) {
            return b.life <= 0 || (b.pos - sp).length() > 500.0f;
        }), bullets.end());

    // ---- Update asteroids ----
    for (auto& a : asteroids) {
        a.pos += a.vel * dt;
        a.rotAngleX += a.rotSpeedX * dt;
        a.rotAngleY += a.rotSpeedY * dt;
        a.rotAngleZ += a.rotSpeedZ * dt;
    }
    asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(),
        [&sp](const AsteroidObj& a) {
            return (a.pos - sp).length() > 600.0f;
        }), asteroids.end());

    // ---- Bullet-Asteroid collisions ----
    std::vector<AsteroidObj> newAsteroids;
    for (auto bit = bullets.begin(); bit != bullets.end();) {
        bool hit = false;
        for (auto ait = asteroids.begin(); ait != asteroids.end();) {
            if (sphereCollision(bit->pos, 0.3f, ait->pos, ait->radius)) {
                ait->health--;
                if (ait->health <= 0) {
                    addExplosion(ait->pos, Vec3(1.0f, 0.7f, 0.3f), 20);
                    score += (int)(100.0f / ait->radius * 5.0f);
                    splitAsteroid(*ait, newAsteroids);
                    ait = asteroids.erase(ait);
                } else {
                    addExplosion(bit->pos, Vec3(1.0f, 1.0f, 0.4f), 5);
                    ++ait;
                }
                hit = true;
                break;
            } else {
                ++ait;
            }
        }
        if (hit) bit = bullets.erase(bit);
        else ++bit;
    }
    
    // Add newly split asteroids to the main list
    asteroids.insert(asteroids.end(), newAsteroids.begin(), newAsteroids.end());

    // ---- Ship-Asteroid collisions ----
    if (invincibleTimer <= 0) {
        for (const auto& a : asteroids) {
            if (sphereCollision(ship.pos, 1.5f, a.pos, a.radius)) {
                lives--;
                invincibleTimer = 2.0f;
                addExplosion(ship.pos, Vec3(1.0f, 0.2f, 0.2f), 30);
                if (lives <= 0) {
                    gameOver = true;
                }
                break;
            }
        }
    }

    // ---- Update particles ----
    for (auto& p : particles) {
        p.pos += p.vel * dt;
        p.life -= dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(),
        [](const Particle& p) { return p.life <= 0; }), particles.end());

    // ---- Reposition stars around ship ----
    for (auto& s : stars) {
        Vec3 rel = s.pos - ship.pos;
        if (rel.x >  500) s.pos.x -= 1000;
        if (rel.x < -500) s.pos.x += 1000;
        if (rel.y >  500) s.pos.y -= 1000;
        if (rel.y < -500) s.pos.y += 1000;
        if (rel.z >  500) s.pos.z -= 1000;
        if (rel.z < -500) s.pos.z += 1000;
    }
}

// ============================================================
// Render
// ============================================================
void SpaceshipGame::render(GameContext& ctx) {
    if (!pipelineReady) {
        ctx.renderer->drawText(glm::vec2(-0.5f, 0.0f), "3D SHADER LOAD FAILED", 0.06f,
            glm::vec4(1, 0, 0, 1));
        return;
    }

    int w = ctx.windowWidth;
    int h = ctx.windowHeight;

    // Flush any 2D draws from beginFrame first
    ctx.renderer->flush();

    // ---- Setup 3D ----
    beginFrame3D(w, h);

    // ---- Camera: chase cam behind and above ship ----
    // Use cameraYaw/Pitch instead of ship's yaw/pitch to provide lag
    Vec3 camForward(
         sinf(cameraYaw) * cosf(cameraPitch),
        -sinf(cameraPitch),
         cosf(cameraYaw) * cosf(cameraPitch)
    );
    camForward = camForward.normalized();

    Vec3 worldUp(0, 1, 0);
    Vec3 camRight = camForward.cross(worldUp).normalized();
    Vec3 camUp = camRight.cross(camForward).normalized();

    float camDist = 8.0f;
    float camHeight = 3.0f;
    Vec3 camTarget = ship.pos + camForward * 5.0f;
    Vec3 camPos = ship.pos - camForward * camDist + camUp * camHeight;

    viewMatrix = Mat4::lookAt(camPos, camTarget, Vec3(0, 1, 0));

    // ---- Draw stars ----
    {
        Mesh starMesh;
        float sz = 0.3f;
        for (const auto& s : stars) {
            Vec3 c(s.brightness, s.brightness, s.brightness);
            pushTri(starMesh,
                s.pos + Vec3(-sz, 0, 0),
                s.pos + Vec3(sz, 0, 0),
                s.pos + Vec3(0, sz, 0),
                c);
        }
        Mat4 identity;
        drawMesh(starMesh, identity, 1.0f);
    }

    // ---- Draw asteroids ----
    for (const auto& a : asteroids) {
        Mat4 model = Mat4::translate(a.pos) *
                     Mat4::rotateX(a.rotAngleX) *
                     Mat4::rotateY(a.rotAngleY) *
                     Mat4::rotateZ(a.rotAngleZ);
        drawMesh(a.mesh, model, 1.0f);

        // Health bar above asteroid
        if (a.health < a.maxHealth) {
            float healthPct = (float)a.health / a.maxHealth;
            float barW = a.radius * 1.5f;
            float barH = 0.3f;
            Vec3 barPos = a.pos + Vec3(0, a.radius + 1.0f, 0);

            Mesh healthMesh;
            // Background
            pushTri(healthMesh,
                barPos + Vec3(-barW/2, 0, 0),
                barPos + Vec3(barW/2, 0, 0),
                barPos + Vec3(barW/2, barH, 0),
                Vec3(0.3f, 0.1f, 0.1f));
            pushTri(healthMesh,
                barPos + Vec3(-barW/2, 0, 0),
                barPos + Vec3(barW/2, barH, 0),
                barPos + Vec3(-barW/2, barH, 0),
                Vec3(0.3f, 0.1f, 0.1f));

            // Foreground
            float fgW = barW * healthPct;
            Vec3 hCol = healthPct > 0.5f ? Vec3(0, 1, 0) : Vec3(1, healthPct * 2, 0);
            pushTri(healthMesh,
                barPos + Vec3(-barW/2, 0, 0),
                barPos + Vec3(-barW/2 + fgW, 0, 0),
                barPos + Vec3(-barW/2 + fgW, barH, 0),
                hCol);
            pushTri(healthMesh,
                barPos + Vec3(-barW/2, 0, 0),
                barPos + Vec3(-barW/2 + fgW, barH, 0),
                barPos + Vec3(-barW/2, barH, 0),
                hCol);

            Mat4 identity;
            drawMesh(healthMesh, identity, 0.9f);
        }
    }

    // ---- Draw bullets ----
    for (const auto& b : bullets) {
        Vec3 dir = b.vel.normalized();
        float bulletYaw = atan2f(dir.x, dir.z);
        float bulletPitch = asinf(-dir.y);

        Mat4 model = Mat4::translate(b.pos) *
                     Mat4::rotateY(bulletYaw) *
                     Mat4::rotateX(bulletPitch) *
                     Mat4::scale(1.0f, 1.0f, 2.0f);
        drawMesh(bulletMesh, model, 1.0f);
    }

    // ---- Draw particles ----
    {
        Mesh particleMesh;
        for (const auto& p : particles) {
            float alpha = p.life / p.maxLife;
            float sz = 0.3f * alpha;
            Vec3 c = p.color * alpha;
            pushTri(particleMesh,
                p.pos + Vec3(-sz, -sz, 0),
                p.pos + Vec3(sz, -sz, 0),
                p.pos + Vec3(0, sz, 0),
                c);
        }
        if (!particleMesh.vertices.empty()) {
            Mat4 identity;
            drawMesh(particleMesh, identity, 1.0f);
        }
    }

    // ---- Draw ship ----
    if (!gameOver && (invincibleTimer <= 0 || (int)(invincibleTimer * 10) % 2 == 0)) {
        Mat4 shipModel = Mat4::translate(ship.pos) *
                         Mat4::rotateY(ship.yaw) *
                         Mat4::rotateX(ship.pitch) *
                         Mat4::rotateZ(ship.roll);
        drawMesh(shipMesh, shipModel, 1.0f);

        // Engine flame attached to the ship
        {
            float flicker = 0.8f + 0.2f * sinf(totalTime * 30.0f);
            float thrustLen = 1.0f + ship.speedIncrease * 0.05f;
            thrustLen *= flicker;

            Mesh flameMesh;
            Vec3 engineBase(0, 0, -1.2f);
            Vec3 flameEnd(0, 0, -1.2f - thrustLen);
            Vec3 col1(1.0f, 0.6f, 0.0f);
            Vec3 col2(1.0f, 0.2f, 0.0f);

            float fw = 0.3f;
            // Horizontal fins of the flame
            pushTri(flameMesh,
                engineBase + Vec3(fw, 0, 0),
                engineBase + Vec3(-fw, 0, 0),
                flameEnd,
                col1, col1, col2);
            // Vertical fins of the flame
            pushTri(flameMesh,
                engineBase + Vec3(0, fw, 0),
                engineBase + Vec3(0, -fw, 0),
                flameEnd,
                col1, col1, col2);

            drawMesh(flameMesh, shipModel, 0.9f);
        }
    }

    // ---- Disable depth test, restore for 2D HUD ----
    glDisable(GL_DEPTH_TEST);

    // ---- 2D HUD ----
    renderHUD(ctx.renderer);
}

void SpaceshipGame::renderHUD(Renderer* r) {
    // The Renderer uses [-1, 1] normalized coords
    // Flush the 3D state and switch back to 2D

    // Score (top-left)
    std::string scoreStr = "SCORE " + std::to_string(score);
    r->drawText(glm::vec2(-0.95f, 0.90f), scoreStr, 0.045f,
        glm::vec4(1, 1, 1, 1));

    // Speed (below score)
    int speedDisp = (int)(ship.forwardSpeed * 10);
    std::string speedStr = "SPEED " + std::to_string(speedDisp);
    r->drawText(glm::vec2(-0.95f, 0.82f), speedStr, 0.04f,
        glm::vec4(0.5f, 1.0f, 0.5f, 1));

    // Lives (top-right) - draw small triangles
    for (int i = 0; i < lives; i++) {
        float lx = 0.80f - i * 0.08f;
        float ly = 0.90f;
        float sz = 0.015f;
        glm::vec4 col(0.0f, 1.0f, 0.5f, 1.0f);
        r->drawRect(glm::vec2(lx - sz, ly - sz), glm::vec2(sz * 2, sz * 2), col);
    }

    // Crosshair (center)
    float csz = 0.02f;
    glm::vec4 cCol(0.0f, 1.0f, 0.5f, 0.6f);
    r->drawRect(glm::vec2(-csz * 3, -0.001f), glm::vec2(csz * 2, 0.002f), cCol);
    r->drawRect(glm::vec2(csz, -0.001f), glm::vec2(csz * 2, 0.002f), cCol);
    r->drawRect(glm::vec2(-0.001f, -csz * 3), glm::vec2(0.002f, csz * 2), cCol);
    r->drawRect(glm::vec2(-0.001f, csz), glm::vec2(0.002f, csz * 2), cCol);

    if (gameOver) {
        r->drawText(glm::vec2(-0.35f, 0.1f), "GAME OVER", 0.08f,
            glm::vec4(1, 0, 0, 1));
        r->drawText(glm::vec2(-0.25f, -0.05f), "SCORE " + std::to_string(score), 0.05f,
            glm::vec4(1, 1, 1, 1));
        r->drawText(glm::vec2(-0.35f, -0.2f), "PRESS R TO RESTART", 0.04f,
            glm::vec4(0.7f, 0.7f, 0.7f, 1));
    }
}

// ============================================================
// Game helpers
// ============================================================
void SpaceshipGame::spawnAsteroid() {
    AsteroidObj a;

    // Spawn ahead and to the sides of the ship
    Vec3 forward(
         sinf(ship.yaw) * cosf(ship.pitch),
        -sinf(ship.pitch),
         cosf(ship.yaw) * cosf(ship.pitch)
    );
    forward = forward.normalized();

    Vec3 right = forward.cross(Vec3(0, 1, 0)).normalized();
    Vec3 up(0, 1, 0);

    float dist = randF(80.0f, 250.0f);
    float lateralOff = randF(-80.0f, 80.0f);
    float verticalOff = randF(-40.0f, 40.0f);

    a.pos = ship.pos + forward * dist + right * lateralOff + up * verticalOff;
    a.radius = randF(2.0f, 6.0f);
    a.health = std::max(1, (int)(a.radius / 1.5f));
    a.maxHealth = a.health;

    a.vel = Vec3(randF(-5, 5), randF(-5, 5), randF(-5, 5));

    a.rotAngleX = randF(0, 6.28f);
    a.rotAngleY = randF(0, 6.28f);
    a.rotAngleZ = randF(0, 6.28f);
    a.rotSpeedX = randF(-2, 2);
    a.rotSpeedY = randF(-2, 2);
    a.rotSpeedZ = randF(-1, 1);

    int detail = a.radius > 4.0f ? 6 : 4;
    a.mesh = makeAsteroidMesh(a.radius, detail);

    asteroids.push_back(a);
}

void SpaceshipGame::splitAsteroid(const AsteroidObj& a, std::vector<AsteroidObj>& newAsteroids) {
    if (a.radius < 2.5f) return;

    float newRadius = a.radius * 0.55f;
    for (int i = 0; i < 2; i++) {
        AsteroidObj child;
        child.pos = a.pos + Vec3(randF(-1, 1), randF(-1, 1), randF(-1, 1));
        child.radius = newRadius;
        child.health = std::max(1, (int)(newRadius / 1.5f));
        child.maxHealth = child.health;
        child.vel = Vec3(
            a.vel.x + randF(-10, 10),
            a.vel.y + randF(-10, 10),
            a.vel.z + randF(-10, 10)
        );
        child.rotAngleX = 0;
        child.rotAngleY = 0;
        child.rotAngleZ = 0;
        child.rotSpeedX = randF(-3, 3);
        child.rotSpeedY = randF(-3, 3);
        child.rotSpeedZ = randF(-2, 2);
        int detail = child.radius > 3.0f ? 5 : 4;
        child.mesh = makeAsteroidMesh(child.radius, detail);
        newAsteroids.push_back(child);
    }
}

void SpaceshipGame::addExplosion(const Vec3& pos, const Vec3& color, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.pos = pos;
        float angle1 = randF(0, 2 * 3.14159f);
        float angle2 = randF(-3.14159f / 2, 3.14159f / 2);
        float speed = randF(5, 30);
        p.vel = Vec3(
            cosf(angle1) * cosf(angle2) * speed,
            sinf(angle2) * speed,
            sinf(angle1) * cosf(angle2) * speed
        );
        p.life = randF(0.3f, 0.8f);
        p.maxLife = p.life;
        p.color = color;
        particles.push_back(p);
    }
}

bool SpaceshipGame::sphereCollision(const Vec3& a, float ra, const Vec3& b, float rb) {
    return (a - b).length() < (ra + rb);
}
