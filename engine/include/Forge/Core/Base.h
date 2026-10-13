#pragma once
// Common includes and aliases used throughout Forge.

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <utility>

namespace Forge {

template <class T> using Ref = std::shared_ptr<T>;
template <class T> using Scope = std::unique_ptr<T>;

template <class T, class... Args>
Ref<T> MakeRef(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

template <class T, class... Args>
Scope<T> MakeScope(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

} // namespace Forge

#define FORGE_UNUSED(x) (void)(x)
