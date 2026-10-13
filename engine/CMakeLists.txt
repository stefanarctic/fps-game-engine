# ---------------------------------------------------------------------------
# Forge runtime: a reusable static library.
# Link against `Forge::Engine` and include <Forge/...> headers.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FORGE_ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(ForgeEngine STATIC ${FORGE_ENGINE_SOURCES})
add_library(Forge::Engine ALIAS ForgeEngine)

target_include_directories(ForgeEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ForgeEngine PUBLIC glfw glad glm imgui stb)

if(WIN32)
    target_link_libraries(ForgeEngine PUBLIC opengl32)
endif()

# Fallback location of the asset folder when the executable is run from the build tree.
target_compile_definitions(ForgeEngine PUBLIC
    GLFW_INCLUDE_NONE
    FORGE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

if(MSVC)
    target_compile_options(ForgeEngine PRIVATE /W3)
endif()
