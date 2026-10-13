# Forge Engine + Arena FPS

A from-scratch C++17 game engine built as a **reusable static library**, a **Unity-style editor**, and a
complete **first-person shooter** made with it.

- **ECS** core (sparse-set registry, systems, reflection-driven components)
- **Renderer**: PBR forward shading, directional shadow maps, **SSAO**, HDR + bloom, ACES tone mapping, FXAA, procedural sky, fog
- **Model loaders**: Wavefront **OBJ/MTL** and **glTF 2.0** (`.gltf` + `.bin` / embedded, and binary `.glb`)
- **Editor**: dockable Hierarchy, Scene view (gizmos, picking, grid), Game view, Inspector, Project browser, Console, Lighting; play mode, undo/redo, scene save/load
- **Game**: wave-based arena shooter with hovering drones and four weapons (assault rifle, shotgun, scoped sniper rifle, rocket launcher) with aim-down-sights. Explosive barrels burn and chain-react. Also pickups and a HUD.
- **Pause menu** (Resume / Settings / Quit) with persistent settings: sensitivity, invert Y, FOV, VSync, fullscreen, graphics toggles

```
build.bat                                  # build (MSVC + CMake + Ninja), Release by default
build\Release\bin\FPSGame.exe              # editor (press Play / Ctrl+P)
build\Release\bin\FPSGame.exe --game       # standalone game
build\Release\bin\ForgeTests.exe           # test suite
```

**Everything is documented in [DOCUMENTATION.md](DOCUMENTATION.md).**
