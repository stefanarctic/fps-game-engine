# Forge unit / integration tests (no external framework).
add_executable(ForgeTests main.cpp)
target_link_libraries(ForgeTests PRIVATE ForgeEngine)
if(MSVC)
    target_compile_options(ForgeTests PRIVATE /W3)
endif()
add_test(NAME ForgeTests COMMAND ForgeTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
