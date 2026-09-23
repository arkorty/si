Retro Console - Virtual Console with Space Invaders
====================================================

A modern OpenGL 3.3 Core Profile virtual console with Space Invaders clone.

Requirements (macOS with Homebrew)
----------------------------------
- CMake 3.10+
- GLFW 3.3+
- GLEW 2.1+
- GLM (header-only math library)
- C++17 compatible compiler (clang++ from Xcode Command Line Tools)

Installation
------------
1. Install Homebrew if not already installed:
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

2. Install dependencies:
   brew install cmake glfw glew glm

Building
--------
mkdir build
cd build
cmake ..
make

Running
-------
cd build
./RetroConsole

Controls (Menu)
---------------
- UP / W: Navigate up
- DOWN / S: Navigate down
- ENTER / SPACE: Select game
- ESC: Quit console

Controls (Space Invaders)
-------------------------
- LEFT / A: Move left
- RIGHT / D: Move right
- SPACE / UP / W: Shoot
- ESC: Return to menu
- R: Restart game (after Game Over or Victory)
- N: Next wave (after Victory)

Gameplay
--------
- Destroy all enemies to win
- Enemies move in discrete grid steps (classic style)
- Enemies drop down when they hit screen edges
- Enemies speed up as their numbers thin out
- If enemies reach your ship: Game Over
- Score: 10-30 points per enemy (top rows worth more)
- 3 lives; bullets can kill you; wave increases difficulty

Architecture
------------
include/
  Game.h              - IGame interface (all games implement this)
  Renderer.h          - Modern OpenGL 3.3 Core renderer
  InputManager.h      - Keyboard input + GameContext struct

src/
  main.cpp            - Entry point
  Console.cpp/h       - Virtual console: menu, game lifecycle, window/input
  Renderer.cpp        - Shader loading, VAO/VBO/EBO batching, draw calls
  games/
    SpaceInvaders.cpp/h - Space Invaders implementation

shaders/
  sprite.vert         - Vertex shader (projection transform)
  sprite.frag         - Fragment shader (vertex color output)

Adding a New Game
-----------------
1. Create src/games/MyGame.h and MyGame.cpp
2. Implement the IGame interface (see SpaceInvaders.h for reference)
3. Register it in Console::init() in src/Console.cpp:
     registerGame(std::make_unique<MyGame>());
4. Add MyGame.cpp to CMakeLists.txt sources

Key Design Notes
----------------
- OpenGL 3.3 Core Profile (no deprecated fixed-function pipeline)
- All drawing uses drawRect() - colored quads batched into a single draw call
- Positions are in NDC [-1, 1]; pos parameter is the bottom-left corner
- Delta-time based movement (frame-rate independent)
- Vertex colors, no textures needed (textures can be added later)
- GLEW required for OpenGL function loading on macOS

Troubleshooting
---------------
If CMake can't find GLFW/GLEW:
  brew install pkg-config
  Then re-run cmake

If you see "GL error" messages in the console, note the hex code:
  0x0500 = GL_INVALID_ENUM
  0x0501 = GL_INVALID_VALUE
  0x0502 = GL_INVALID_OPERATION

On Apple Silicon (M1/M2/M3): works natively.

License
-------
Public domain / educational use
