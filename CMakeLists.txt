cmake_minimum_required(VERSION 3.20)
project(ForgeEngine VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(MSVC)
    add_compile_options(/MP /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Third-party code (GLFW, glad, GLM, Dear ImGui + ImGuizmo, stb)
add_subdirectory(external)

# Forge runtime library (ECS, renderer, assets, physics, input...)
add_subdirectory(engine)

# Forge editor library (Unity-style editor built on top of the runtime)
add_subdirectory(editor)

# The FPS game that uses both libraries
add_subdirectory(game)

# Automated tests (ECS, JSON, serialization, math, physics, model loaders)
option(FORGE_BUILD_TESTS "Build the Forge test suite" ON)
if(FORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
