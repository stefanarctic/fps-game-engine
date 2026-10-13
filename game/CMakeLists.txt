# ---------------------------------------------------------------------------
# "Arena" - the FPS game built with Forge.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FPS_GAME_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_executable(FPSGame ${FPS_GAME_SOURCES})
target_link_libraries(FPSGame PRIVATE ForgeEngine ForgeEditor)

if(MSVC)
    target_compile_options(FPSGame PRIVATE /W3)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT FPSGame)
    set_target_properties(FPSGame PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# Copy the asset folder next to the executable so the build output is self-contained.
add_custom_command(TARGET FPSGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:FPSGame>/assets
    COMMENT "Copying assets next to FPSGame")
