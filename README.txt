si - Virtual Console
====================

A modern OpenGL 3.3 Core Profile virtual console with a BASIC-style
command shell and three arcade games: Space Invaders, Asteroids, and Pong.

Requirements (macOS with Homebrew)
----------------------------------
- CMake 3.10+
- GLFW 3.3+
- GLEW 2.1+
- GLM (header-only math library)
- C++17 compiler (clang++ from Xcode Command Line Tools)

Installation
------------
brew install cmake glfw glew glm

Building
--------
mkdir build && cd build
cmake ..
make

Running
-------
./si

BASIC Shell (Menu)
------------------
Type a game name and press ENTER to play.

Commands:
  LIST              List available games
  RUN <NAME>        Start a game
  <NAME>            Start a game (e.g. ASTEROIDS)
  0 / 1 / 2         Start by program number from LIST
  CLS               Clear screen
  HELP              Show help
  BYE               Quit

Keys:
  ENTER   Execute command
  BACKSPACE  Delete character
  ESC     Quit (at prompt) / return to shell (in game)

Game names are case-insensitive and ignore spaces:
  SPACE INVADERS, SPACEINVADERS, Space Invaders all work.

Controls (All Games)
--------------------
- ESC : Back to BASIC shell

Space Invaders
  LEFT/RIGHT or A/D : Move
  SPACE / UP / W    : Shoot
  R                 : Restart (after game over)
  N                 : Next wave (after victory)

Asteroids
  LEFT/RIGHT or A/D : Rotate ship
  UP / W            : Thrust
  SPACE             : Fire
  P                 : Pause
  R                 : Restart (after game over)

Pong
  W / S       : Left paddle
  UP / DOWN   : Right paddle (2P mode)
  T           : Toggle 1P vs CPU / 2P versus
  R           : Restart (after game over)

Project Structure
-----------------
include/
  Game.h              - IGame interface
  Console.h           - Virtual console + BASIC shell
  Renderer.h          - OpenGL renderer + bitmap font
  InputManager.h      - Keyboard input + GameContext
src/
  main.cpp            - Entry point
  Console.cpp         - BASIC shell, game lifecycle, window
  Renderer.cpp        - Shaders, VAO/VBO batching, drawRect, drawText
  games/
    SpaceInvaders.*   - Space Invaders
    Asteroids.*       - Asteroids
    Pong.*            - Pong (with CPU opponent)
shaders/
  sprite.vert/frag    - Sprite rendering shaders

Adding a New Game
-----------------
1. Create src/games/MyGame.h and MyGame.cpp implementing IGame
2. Register in Console::init():
     registerGame(std::make_unique<MyGame>());
3. Add MyGame.cpp to CMakeLists.txt sources

License
-------
Public domain / educational use
