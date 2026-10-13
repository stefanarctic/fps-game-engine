#pragma once
// Umbrella header for games built on the Forge runtime.

#include "Forge/Core/Application.h"
#include "Forge/Core/Base.h"
#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Game.h"
#include "Forge/Core/Input.h"
#include "Forge/Core/Json.h"
#include "Forge/Core/Log.h"
#include "Forge/Core/Settings.h"
#include "Forge/Core/Time.h"

#include "Forge/Math/Math.h"

#include "Forge/ECS/Reflection.h"
#include "Forge/ECS/Registry.h"
#include "Forge/ECS/System.h"

#include "Forge/Scene/Components.h"
#include "Forge/Scene/CoreSystems.h"
#include "Forge/Scene/Scene.h"
#include "Forge/Scene/SceneSerializer.h"

#include "Forge/Physics/PhysicsWorld.h"

#include "Forge/Assets/AssetManager.h"
#include "Forge/Assets/ModelLoaders.h"

#include "Forge/Renderer/DebugDraw.h"
#include "Forge/Renderer/Material.h"
#include "Forge/Renderer/SceneRenderer.h"

#include "Forge/Runtime/RuntimeLayer.h"
#include "Forge/UI/PauseMenu.h"
