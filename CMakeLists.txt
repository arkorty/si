cmake_minimum_required(VERSION 3.10)
project(RetroConsole)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

if (NOT DEFINED GLM_INCLUDE_DIRS)
    find_path(GLM_INCLUDE_DIRS glm/glm.hpp
        PATHS /opt/homebrew/include /usr/local/include /usr/include
    )
    if (GLM_INCLUDE_DIRS)
        message(STATUS "Found GLM: ${GLM_INCLUDE_DIRS}")
    else()
        message(WARNING "GLM not found - please install: brew install glm")
    endif()
endif()

add_executable(RetroConsole
    src/main.cpp
    src/Console.cpp
    src/Renderer.cpp
    src/games/SpaceInvaders.cpp
)

target_include_directories(RetroConsole PRIVATE
    include
    ${GLFW_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_directories(RetroConsole PRIVATE
    ${GLFW_LIBRARY_DIRS}
    ${GLEW_LIBRARY_DIRS}
)

target_link_libraries(RetroConsole
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLEW_LIBRARIES}
)

target_compile_options(RetroConsole PRIVATE
    ${GLFW_CFLAGS_OTHER}
    ${GLEW_CFLAGS_OTHER}
)

set_target_properties(RetroConsole PROPERTIES
    MACOSX_BUNDLE FALSE
)

file(COPY shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})