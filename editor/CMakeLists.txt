# ---------------------------------------------------------------------------
# Forge editor: Unity-style editor (dock layout, hierarchy, inspector,
# scene/game views, project browser, console). Depends on the runtime only.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FORGE_EDITOR_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

add_library(ForgeEditor STATIC ${FORGE_EDITOR_SOURCES})
add_library(Forge::Editor ALIAS ForgeEditor)

target_include_directories(ForgeEditor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ForgeEditor PUBLIC ForgeEngine)

if(MSVC)
    target_compile_options(ForgeEditor PRIVATE /W3)
endif()
