#pragma once

#include "Game.h"
#include "Renderer.h"
#include "InputManager.h"
#include <vector>
#include <cmath>
#include <random>

class SpaceshipGame : public IGame {
public:
    SpaceshipGame();
    ~SpaceshipGame() override;

    const char* getName() const override { return "Spaceship"; }
    const char* getDescription() const override { return "3D retro triangle mesh space shooter"; }

    void init(GameContext& ctx) override;
    void cleanup(GameContext& ctx) override;
    void onEnter(GameContext& ctx) override;
    void onExit(GameContext& ctx) override;
    void update(GameContext& ctx, float dt) override;
    void render(GameContext& ctx) override;
    void onKey(GameContext& ctx, int key, int action) override;
    void onResize(GameContext& ctx, int width, int height) override;

private:
    // ---- Math types ----
    struct Vec3 {
        float x, y, z;
        Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
        Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
        Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
        Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
        Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
        Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
        Vec3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
        float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
        Vec3 cross(const Vec3& o) const {
            return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
        }
        float length() const { return std::sqrt(x*x + y*y + z*z); }
        Vec3 normalized() const {
            float l = length();
            if (l < 0.0001f) return {0,0,0};
            return {x/l, y/l, z/l};
        }
    };

    struct Mat4 {
        float m[16]; // column-major
        Mat4() { identity(); }
        void identity() {
            for (int i = 0; i < 16; i++) m[i] = 0;
            m[0] = m[5] = m[10] = m[15] = 1;
        }
        float& at(int row, int col) { return m[col * 4 + row]; }
        float at(int row, int col) const { return m[col * 4 + row]; }
        const float* data() const { return m; }

        Mat4 operator*(const Mat4& o) const {
            Mat4 r;
            for (int i = 0; i < 16; i++) r.m[i] = 0;
            for (int col = 0; col < 4; col++)
                for (int row = 0; row < 4; row++)
                    for (int k = 0; k < 4; k++)
                        r.at(row, col) += at(row, k) * o.at(k, col);
            return r;
        }

        Vec3 transformPoint(const Vec3& v) const {
            float w = at(3,0)*v.x + at(3,1)*v.y + at(3,2)*v.z + at(3,3);
            if (std::abs(w) < 0.0001f) w = 0.0001f;
            return {
                (at(0,0)*v.x + at(0,1)*v.y + at(0,2)*v.z + at(0,3)) / w,
                (at(1,0)*v.x + at(1,1)*v.y + at(1,2)*v.z + at(1,3)) / w,
                (at(2,0)*v.x + at(2,1)*v.y + at(2,2)*v.z + at(2,3)) / w
            };
        }

        static Mat4 perspective(float fovY, float aspect, float nearP, float farP) {
            Mat4 r;
            for (int i = 0; i < 16; i++) r.m[i] = 0;
            float tanHalf = std::tan(fovY / 2.0f);
            r.at(0,0) = 1.0f / (aspect * tanHalf);
            r.at(1,1) = 1.0f / tanHalf;
            r.at(2,2) = -(farP + nearP) / (farP - nearP);
            r.at(3,2) = -1.0f;
            r.at(2,3) = -(2.0f * farP * nearP) / (farP - nearP);
            return r;
        }

        static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
            Vec3 f = (target - eye).normalized();
            Vec3 s = f.cross(up).normalized();
            Vec3 u = s.cross(f);
            Mat4 r;
            r.at(0,0) = s.x; r.at(0,1) = s.y; r.at(0,2) = s.z;
            r.at(1,0) = u.x; r.at(1,1) = u.y; r.at(1,2) = u.z;
            r.at(2,0) = -f.x; r.at(2,1) = -f.y; r.at(2,2) = -f.z;
            r.at(0,3) = -s.dot(eye);
            r.at(1,3) = -u.dot(eye);
            r.at(2,3) = f.dot(eye);
            return r;
        }

        static Mat4 translate(const Vec3& v) {
            Mat4 r;
            r.at(0,3) = v.x; r.at(1,3) = v.y; r.at(2,3) = v.z;
            return r;
        }

        static Mat4 rotateY(float angle) {
            Mat4 r;
            float c = std::cos(angle), s = std::sin(angle);
            r.at(0,0) = c;  r.at(0,2) = s;
            r.at(2,0) = -s; r.at(2,2) = c;
            return r;
        }

        static Mat4 rotateX(float angle) {
            Mat4 r;
            float c = std::cos(angle), s = std::sin(angle);
            r.at(1,1) = c;  r.at(1,2) = -s;
            r.at(2,1) = s;  r.at(2,2) = c;
            return r;
        }

        static Mat4 rotateZ(float angle) {
            Mat4 r;
            float c = std::cos(angle), s = std::sin(angle);
            r.at(0,0) = c;  r.at(0,1) = -s;
            r.at(1,0) = s;  r.at(1,1) = c;
            return r;
        }

        static Mat4 scale(float sx, float sy, float sz) {
            Mat4 r;
            r.at(0,0) = sx; r.at(1,1) = sy; r.at(2,2) = sz;
            return r;
        }
    };

    // ---- Mesh: list of triangles (pos + color per vertex) ----
    struct MeshVertex {
        Vec3 pos;
        Vec3 color; // rgb 0-1
    };

    struct Mesh {
        std::vector<MeshVertex> vertices; // every 3 = one triangle
    };

    // ---- 3D GL state (self-contained, doesn't touch Renderer's state) ----
    GLuint shader3D = 0;
    GLuint vao3D = 0, vbo3D = 0;
    GLint locModel = -1, locView = -1, locProj = -1, locAlpha = -1;
    bool pipelineReady = false;

    void init3DPipeline();
    void cleanup3DPipeline();
    GLuint compile3DShader(const char* path, GLenum type);
    void drawMesh(const Mesh& mesh, const Mat4& model, float alpha = 1.0f);
    void beginFrame3D(int w, int h);

    // ---- Mesh generators ----
    Mesh makeShipMesh();
    Mesh makeAsteroidMesh(float radius, int detail);
    Mesh makeBulletMesh();

    // Helper to push a triangle into a mesh
    static void pushTri(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 col);
    static void pushTri(Mesh& m, Vec3 a, Vec3 b, Vec3 c, Vec3 colA, Vec3 colB, Vec3 colC);

    // ---- Game objects ----
    struct ShipState {
        Vec3 pos;
        Vec3 vel;
        float yaw;
        float pitch;
        float roll;
        float forwardSpeed;
        float baseSpeed;
        float speedIncrease;
    } ship;

    struct Bullet {
        Vec3 pos;
        Vec3 vel;
        float life;
    };
    std::vector<Bullet> bullets;
    float shootCooldown;

    struct AsteroidObj {
        Vec3 pos;
        Vec3 vel;
        float radius;
        float rotAngleX, rotAngleY, rotAngleZ;
        float rotSpeedX, rotSpeedY, rotSpeedZ;
        int health;
        int maxHealth;
        Mesh mesh;
    };
    std::vector<AsteroidObj> asteroids;

    struct Star {
        Vec3 pos;
        float brightness;
    };
    std::vector<Star> stars;

    struct Particle {
        Vec3 pos;
        Vec3 vel;
        float life;
        float maxLife;
        Vec3 color;
    };
    std::vector<Particle> particles;

    // ---- Game state ----
    int score;
    int lives;
    bool gameOver;
    float invincibleTimer;
    float spawnTimer;
    float difficulty;
    float totalTime;

    // ---- Prebuilt meshes ----
    Mesh shipMesh;
    Mesh bulletMesh;

    // ---- Camera ----
    Mat4 viewMatrix;
    Mat4 projMatrix;
    float cameraYaw;
    float cameraPitch;

    // ---- Helpers ----
    void resetGame();
    void spawnAsteroid();
    void splitAsteroid(const AsteroidObj& a, std::vector<AsteroidObj>& newAsteroids);
    void addExplosion(const Vec3& pos, const Vec3& color, int count);
    bool sphereCollision(const Vec3& a, float ra, const Vec3& b, float rb);
    float randF(float lo, float hi);

    // ---- HUD (uses Renderer's 2D) ----
    void renderHUD(Renderer* r);

    std::mt19937 rng;
};
