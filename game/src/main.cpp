// Arena FPS entry point.
//
//   FPSGame.exe                 open the Forge editor with the Arena scene
//   FPSGame.exe --game          run the standalone game (no editor)
//   FPSGame.exe --play          open the editor and enter play mode immediately
//   FPSGame.exe --scene <path>  open a specific scene (asset path)
//   --screenshot out.png --frames N   capture the window after N frames and exit
#include "FPSGame.h"

#include <Forge/Editor/EditorLayer.h>
#include <Forge/Forge.h>

int main(int argc, char** argv) {
    Forge::FileSystem::initAssetRoot(argc > 0 ? argv[0] : nullptr);

    Forge::AppConfig defaults;
    defaults.title = "Arena FPS - Forge Engine";
    defaults.width = 1600;
    defaults.height = 900;
    Forge::AppConfig config = Forge::AppConfig::fromArgs(argc, argv, defaults);

    FPS::FPSGame game;
    Forge::Application app(config);
    if (config.editor) app.pushLayer(Forge::MakeScope<Forge::Editor::EditorLayer>(game, config));
    else app.pushLayer(Forge::MakeScope<Forge::RuntimeLayer>(game, config.scene));
    return app.run();
}
