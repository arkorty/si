#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

struct Vertex {
    glm::vec2 pos;
    glm::vec4 color;
};

class Shader {
public:
    Shader() : program(0) {}
    ~Shader() { if (program) glDeleteProgram(program); }

    bool loadFromFile(const std::string& vertPath, const std::string& fragPath);
    void use() const { glUseProgram(program); }
    GLuint getProgram() const { return program; }

    void setMat4(const char* name, const glm::mat4& mat) const;
    void setVec4(const char* name, const glm::vec4& vec) const;
    void setInt(const char* name, int value) const;

private:
    GLuint program;
    mutable std::unordered_map<std::string, GLint> uniformCache;

    GLint getUniformLocation(const char* name) const;
    static GLuint compileShader(GLenum type, const std::string& source);
    static std::string readFile(const std::string& path);
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init();
    void shutdown();

    void beginFrame();
    void endFrame();

    void setProjection(const glm::mat4& proj) { projection = proj; }

    void drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color, float rotation = 0.0f);
    // Draw text with built-in 5x7 bitmap font. pos = bottom-left of first character.
    void drawText(const glm::vec2& pos, const std::string& text, float charSize,
                  const glm::vec4& color);
    void flush();

private:
    void initBuffers();
    // Returns 5-bit row pattern for a character at row 0-6 (top to bottom), or -1 if unsupported
    static int glyphRow(char c, int row);

    Shader spriteShader;

    GLuint spriteVAO = 0, spriteVBO = 0, spriteEBO = 0;
    std::vector<Vertex> spriteVertices;

    glm::mat4 projection;
    GLuint maxQuads = 10000;
};
