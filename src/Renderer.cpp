#include "Renderer.h"
#include <fstream>
#include <sstream>
#include <iostream>

// Check OpenGL errors after important calls - helps find silent failures
static void checkGL(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "GL error after [" << label << "]: 0x" << std::hex << err << std::dec << std::endl;
    }
}

GLuint Shader::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Shader::loadFromFile(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSource = readFile(vertPath);
    std::string fragSource = readFile(fragPath);
    if (vertSource.empty() || fragSource.empty()) return false;

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSource);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSource);
    if (!vertShader || !fragShader) return false;

    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader linking failed: " << infoLog << std::endl;
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return true;
}

GLint Shader::getUniformLocation(const char* name) const {
    auto it = uniformCache.find(name);
    if (it != uniformCache.end()) return it->second;
    GLint location = glGetUniformLocation(program, name);
    if (location == -1) {
        std::cerr << "Uniform not found: " << name << std::endl;
    }
    uniformCache[name] = location;
    return location;
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec4(const char* name, const glm::vec4& vec) const {
    glUniform4fv(getUniformLocation(name), 1, &vec[0]);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(getUniformLocation(name), value);
}

Renderer::Renderer() {
    projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    spriteVertices.reserve(maxQuads * 4);
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW init failed: " << glewGetErrorString(err) << std::endl;
        return false;
    }
    // GLEW sets a spurious GL error on core profile - clear it
    glGetError();

    std::cerr << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cerr << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    if (!spriteShader.loadFromFile("shaders/sprite.vert", "shaders/sprite.frag")) {
        std::cerr << "Failed to load sprite shader" << std::endl;
        return false;
    }

    initBuffers();
    checkGL("initBuffers");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    return true;
}

void Renderer::initBuffers() {
    // Pre-generate index buffer for maxQuads quads (4 verts, 6 indices each)
    std::vector<GLuint> indices;
    indices.reserve(maxQuads * 6);
    for (GLuint i = 0; i < maxQuads; ++i) {
        GLuint b = i * 4;
        indices.push_back(b + 0);
        indices.push_back(b + 1);
        indices.push_back(b + 2);
        indices.push_back(b + 2);
        indices.push_back(b + 3);
        indices.push_back(b + 0);
    }

    glGenVertexArrays(1, &spriteVAO);
    glGenBuffers(1, &spriteVBO);
    glGenBuffers(1, &spriteEBO);

    glBindVertexArray(spriteVAO);

    glBindBuffer(GL_ARRAY_BUFFER, spriteVBO);
    glBufferData(GL_ARRAY_BUFFER, maxQuads * 4 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spriteEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

    // pos: vec2 at location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    // color: vec4 at location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    glBindVertexArray(0);
}

void Renderer::shutdown() {
    if (spriteVAO) glDeleteVertexArrays(1, &spriteVAO);
    if (spriteVBO) glDeleteBuffers(1, &spriteVBO);
    if (spriteEBO) glDeleteBuffers(1, &spriteEBO);
    spriteVAO = spriteVBO = spriteEBO = 0;
}

void Renderer::beginFrame() {
    spriteVertices.clear();
    glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::endFrame() {
    flush();
    checkGL("flush");
}

void Renderer::drawRect(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color, float rotation) {
    if (spriteVertices.size() + 4 > maxQuads * 4) flush();

    // pos = bottom-left corner of the rect
    // Build transform: scale unit quad to size, rotate around center, move center to pos + size/2
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f, 0.0f));
    model = glm::rotate(model, rotation, glm::vec3(0, 0, 1));
    model = glm::scale(model, glm::vec3(size.x, size.y, 1.0f));

    // Unit quad corners [-0.5, 0.5]
    glm::vec2 corners[4] = {
        {-0.5f, -0.5f},
        { 0.5f, -0.5f},
        { 0.5f,  0.5f},
        {-0.5f,  0.5f},
    };

    for (int i = 0; i < 4; ++i) {
        glm::vec4 transformed = model * glm::vec4(corners[i], 0.0f, 1.0f);
        Vertex v;
        v.pos = glm::vec2(transformed.x, transformed.y);
        v.color = color;
        spriteVertices.push_back(v);
    }
}

void Renderer::flush() {
    if (spriteVertices.empty()) return;

    spriteShader.use();
    spriteShader.setMat4("uProjection", projection);

    glBindVertexArray(spriteVAO);
    glBindBuffer(GL_ARRAY_BUFFER, spriteVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, spriteVertices.size() * sizeof(Vertex), spriteVertices.data());

    GLuint count = static_cast<GLuint>(spriteVertices.size() / 4 * 6);
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    spriteVertices.clear();
}

// --- 5x7 bitmap font ---
// Each glyph is 7 rows; each row is 5 bits (bit 4 = leftmost pixel).
// Stored as 7 bytes per character, indexed from ' ' (space) to 'z'.

static const unsigned char FONT_DATA[] = {
    // ' ' (0x20)
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // '!' 0x21
    0x04,0x04,0x04,0x04,0x04,0x00,0x04,
    // '"' 0x22
    0x0A,0x0A,0x00,0x00,0x00,0x00,0x00,
    // '#' 0x23
    0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00,
    // '$' 0x24
    0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04,
    // '%' 0x25
    0x18,0x19,0x02,0x04,0x08,0x13,0x03,
    // '&' 0x26
    0x0C,0x12,0x14,0x08,0x15,0x12,0x0D,
    // '\'' 0x27
    0x04,0x04,0x08,0x00,0x00,0x00,0x00,
    // '(' 0x28
    0x02,0x04,0x08,0x08,0x08,0x04,0x02,
    // ')' 0x29
    0x08,0x04,0x02,0x02,0x02,0x04,0x08,
    // '*' 0x2A
    0x00,0x04,0x15,0x0E,0x15,0x04,0x00,
    // '+' 0x2B
    0x00,0x04,0x04,0x1F,0x04,0x04,0x00,
    // ',' 0x2C
    0x00,0x00,0x00,0x00,0x00,0x04,0x08,
    // '-' 0x2D
    0x00,0x00,0x00,0x1F,0x00,0x00,0x00,
    // '.' 0x2E
    0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,
    // '/' 0x2F
    0x01,0x01,0x02,0x04,0x08,0x10,0x10,
    // '0'-'9' 0x30-0x39
    0x0E,0x11,0x13,0x15,0x19,0x11,0x0E, // 0
    0x04,0x0C,0x04,0x04,0x04,0x04,0x0E, // 1
    0x0E,0x11,0x01,0x02,0x04,0x08,0x1F, // 2
    0x1F,0x02,0x04,0x02,0x01,0x11,0x0E, // 3
    0x02,0x06,0x0A,0x12,0x1F,0x02,0x02, // 4
    0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E, // 5
    0x06,0x08,0x10,0x1E,0x11,0x11,0x0E, // 6
    0x1F,0x01,0x02,0x04,0x08,0x08,0x08, // 7
    0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E, // 8
    0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C, // 9
    // ':' 0x3A
    0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00,
    // ';' 0x3B
    0x00,0x0C,0x0C,0x00,0x04,0x04,0x08,
    // '<' 0x3C
    0x02,0x04,0x08,0x10,0x08,0x04,0x02,
    // '=' 0x3D
    0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,
    // '>' 0x3E
    0x08,0x04,0x02,0x01,0x02,0x04,0x08,
    // '?' 0x3F
    0x0E,0x11,0x01,0x02,0x04,0x00,0x04,
    // '@' 0x40
    0x0E,0x11,0x17,0x15,0x17,0x10,0x0E,
    // 'A'-'Z' 0x41-0x5A
    0x0E,0x11,0x11,0x1F,0x11,0x11,0x11, // A
    0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E, // B
    0x0E,0x11,0x10,0x10,0x10,0x11,0x0E, // C
    0x1C,0x12,0x11,0x11,0x11,0x12,0x1C, // D
    0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F, // E
    0x1F,0x10,0x10,0x1E,0x10,0x10,0x10, // F
    0x0E,0x11,0x10,0x17,0x11,0x11,0x0F, // G
    0x11,0x11,0x11,0x1F,0x11,0x11,0x11, // H
    0x0E,0x04,0x04,0x04,0x04,0x04,0x0E, // I
    0x07,0x02,0x02,0x02,0x02,0x12,0x0C, // J
    0x11,0x12,0x14,0x18,0x14,0x12,0x11, // K
    0x10,0x10,0x10,0x10,0x10,0x10,0x1F, // L
    0x11,0x1B,0x15,0x15,0x11,0x11,0x11, // M
    0x11,0x11,0x19,0x15,0x13,0x11,0x11, // N
    0x0E,0x11,0x11,0x11,0x11,0x11,0x0E, // O
    0x1E,0x11,0x11,0x1E,0x10,0x10,0x10, // P
    0x0E,0x11,0x11,0x11,0x15,0x12,0x0D, // Q
    0x1E,0x11,0x11,0x1E,0x14,0x12,0x11, // R
    0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E, // S
    0x1F,0x04,0x04,0x04,0x04,0x04,0x04, // T
    0x11,0x11,0x11,0x11,0x11,0x11,0x0E, // U
    0x11,0x11,0x11,0x11,0x11,0x0A,0x04, // V
    0x11,0x11,0x11,0x15,0x15,0x1B,0x11, // W
    0x11,0x11,0x0A,0x04,0x0A,0x11,0x11, // X
    0x11,0x11,0x0A,0x04,0x04,0x04,0x04, // Y
    0x1F,0x01,0x02,0x04,0x08,0x10,0x1F, // Z
    // '[' 0x5B
    0x0E,0x08,0x08,0x08,0x08,0x08,0x0E,
    // '\\' 0x5C
    0x10,0x10,0x08,0x04,0x02,0x01,0x01,
    // ']' 0x5D
    0x0E,0x02,0x02,0x02,0x02,0x02,0x0E,
    // '^' 0x5E
    0x04,0x0A,0x11,0x00,0x00,0x00,0x00,
    // '_' 0x5F
    0x00,0x00,0x00,0x00,0x00,0x00,0x1F,
};

// Lookup glyph row; returns -1 for unsupported characters
int Renderer::glyphRow(char c, int row) {
    if (row < 0 || row > 6) return -1;

    // Map lowercase to uppercase
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';

    int idx = (int)c - (int)' ';
    if (idx < 0 || idx >= (int)(sizeof(FONT_DATA) / 7)) return -1;

    return FONT_DATA[idx * 7 + row];
}

void Renderer::drawText(const glm::vec2& pos, const std::string& text, float charSize,
                        const glm::vec4& color) {
    // Each pixel is charSize/7 in each dimension (5 wide, 7 tall glyph)
    float px = charSize / 7.0f; // pixel size
    float advance = px * 6.0f;  // 5 pixels + 1 pixel spacing

    float cx = pos.x;
    float cy = pos.y;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (c == '\n') {
            cx = pos.x;
            cy -= px * 9.0f; // line height
            continue;
        }

        for (int row = 0; row < 7; ++row) {
            int bits = glyphRow(c, row);
            if (bits < 0) continue;

            for (int col = 0; col < 5; ++col) {
                // bit 4 = leftmost column
                if (bits & (1 << (4 - col))) {
                    float x = cx + col * px;
                    // row 0 = top of glyph, so flip vertically
                    float y = cy + (6 - row) * px;
                    drawRect(glm::vec2(x, y), glm::vec2(px, px), color);
                }
            }
        }

        cx += advance;
    }
}
