#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

// Small filesystem helpers. "Asset paths" are relative to the asset root
// (e.g. "models/crate.obj"); absolute paths are passed through untouched.
namespace FileSystem {

bool readText(const std::string& path, std::string& out);
bool readBinary(const std::string& path, std::vector<uint8_t>& out);
bool writeText(const std::string& path, const std::string& text);
bool exists(const std::string& path);
bool isDirectory(const std::string& path);
bool createDirectories(const std::string& path);
int64_t lastWriteTime(const std::string& path);

std::string directory(const std::string& path);      // "a/b/c.txt" -> "a/b"
std::string filename(const std::string& path);       // "a/b/c.txt" -> "c.txt"
std::string stem(const std::string& path);           // "a/b/c.txt" -> "c"
std::string extension(const std::string& path);      // "a/b/c.TXT" -> ".txt" (lower case)
std::string join(const std::string& a, const std::string& b);
std::string normalize(const std::string& path);      // forward slashes, collapsed ".."

struct DirEntry {
    std::string name;
    std::string path; // asset path (relative to the asset root)
    bool isDirectory = false;
};
// Lists a directory given as an asset path. Directories first, then files, sorted by name.
std::vector<DirEntry> listAssets(const std::string& assetDir);
// Recursively lists all files under the asset root whose extension is in `exts` (e.g. {".obj", ".gltf"}).
std::vector<std::string> findAssets(const std::vector<std::string>& exts);

// --- Asset root ---------------------------------------------------------
// Searches next to the executable, the working directory and the compile-time
// fallback FORGE_ASSET_DIR for an "assets" folder.
void initAssetRoot(const char* argv0);
const std::string& assetRoot();
std::string resolve(const std::string& assetPath);   // asset path -> absolute path
std::string toAssetPath(const std::string& absolutePath);

} // namespace FileSystem
} // namespace Forge
