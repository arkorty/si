cmake_minimum_required(VERSION 3.10)
project(si)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
elseif(MSVC)
    add_compile_options(/W3 /WX-)
endif()

find_package(OpenGL REQUIRED)

# Attempt standard find_package first (works with vcpkg, MSYS2, etc.)
find_package(glfw3 QUIET)
find_package(GLEW QUIET)
find_package(glm QUIET)

# Fallback to PkgConfig for Linux / macOS if standard find_package fails
find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    if (NOT glfw3_FOUND AND NOT GLFW3_FOUND)
        pkg_check_modules(GLFW_PKG QUIET glfw3)
    endif()
    if (NOT GLEW_FOUND)
        pkg_check_modules(GLEW_PKG QUIET glew)
    endif()
endif()

# Fallback for GLM header-only
if (NOT glm_FOUND AND NOT GLM_FOUND)
    find_path(GLM_INCLUDE_DIRS glm/glm.hpp
        PATHS /opt/homebrew/include /usr/local/include /usr/include "C:/vcpkg/installed/x64-windows/include"
    )
    if (NOT GLM_INCLUDE_DIRS)
        message(WARNING "GLM not found - please install glm")
    endif()
endif()

add_executable(si
    src/main.cpp
    src/Console.cpp
    src/Renderer.cpp
    src/games/SpaceInvaders.cpp
    src/games/Asteroids.cpp
    src/games/Pong.cpp
    src/games/Spaceship.cpp
)

target_include_directories(si PRIVATE include)

# Link OpenGL
target_link_libraries(si PRIVATE ${OPENGL_LIBRARIES})

# Link GLFW
if (glfw3_FOUND OR GLFW3_FOUND)
    target_link_libraries(si PRIVATE glfw)
elseif (GLFW_PKG_FOUND)
    target_include_directories(si PRIVATE ${GLFW_PKG_INCLUDE_DIRS})
    target_link_directories(si PRIVATE ${GLFW_PKG_LIBRARY_DIRS})
    target_link_libraries(si PRIVATE ${GLFW_PKG_LIBRARIES})
    target_compile_options(si PRIVATE ${GLFW_PKG_CFLAGS_OTHER})
else()
    message(FATAL_ERROR "GLFW3 not found. Please install it.")
endif()

# Link GLEW
if (GLEW_FOUND)
    target_link_libraries(si PRIVATE GLEW::GLEW)
elseif (GLEW_PKG_FOUND)
    target_include_directories(si PRIVATE ${GLEW_PKG_INCLUDE_DIRS})
    target_link_directories(si PRIVATE ${GLEW_PKG_LIBRARY_DIRS})
    target_link_libraries(si PRIVATE ${GLEW_PKG_LIBRARIES})
    target_compile_options(si PRIVATE ${GLEW_PKG_CFLAGS_OTHER})
else()
    message(FATAL_ERROR "GLEW not found. Please install it.")
endif()

# Link GLM
if (glm_FOUND OR GLM_FOUND)
    target_link_libraries(si PRIVATE glm::glm)
elseif (GLM_INCLUDE_DIRS)
    target_include_directories(si PRIVATE ${GLM_INCLUDE_DIRS})
endif()

# Disable console window on Windows release builds
if (WIN32)
    set_target_properties(si PROPERTIES WIN32_EXECUTABLE FALSE)
    # Ensure math constants like M_PI are defined just in case
    target_compile_definitions(si PRIVATE _USE_MATH_DEFINES)
endif()

# Copy shaders to build directory
file(COPY shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})