#include "Forge/Core/FileSystem.h"
#include "Forge/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Forge {
namespace FileSystem {

namespace {
std::string g_AssetRoot;

std::string toGeneric(const fs::path& p) { return p.generic_string(); }
} // namespace

bool readText(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

bool readBinary(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file) return false;
    std::streamsize size = file.tellg();
    if (size < 0) return false;
    file.seekg(0, std::ios::beg);
    out.resize((size_t)size);
    if (size > 0) file.read(reinterpret_cast<char*>(out.data()), size);
    return (bool)file;
}

bool writeText(const std::string& path, const std::string& text) {
    std::error_code ec;
    fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(text.data(), (std::streamsize)text.size());
    return (bool)file;
}

bool exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool isDirectory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool createDirectories(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

int64_t lastWriteTime(const std::string& path) {
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    if (ec) return 0;
    return (int64_t)t.time_since_epoch().count();
}

std::string directory(const std::string& path) { return toGeneric(fs::path(path).parent_path()); }
std::string filename(const std::string& path) { return fs::path(path).filename().string(); }
std::string stem(const std::string& path) { return fs::path(path).stem().string(); }

std::string extension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return toGeneric(fs::path(a) / fs::path(b));
}

std::string normalize(const std::string& path) { return toGeneric(fs::path(path).lexically_normal()); }

void initAssetRoot(const char* argv0) {
    std::vector<fs::path> candidates;
    std::error_code ec;
    if (argv0) {
        fs::path exe = fs::absolute(fs::path(argv0), ec).parent_path();
        candidates.push_back(exe / "assets");
        candidates.push_back(exe / ".." / "assets");
        candidates.push_back(exe / ".." / ".." / "assets");
    }
    candidates.push_back(fs::current_path(ec) / "assets");
#ifdef FORGE_ASSET_DIR
    candidates.push_back(fs::path(FORGE_ASSET_DIR));
#endif
    // Prefer the source asset folder when present so editor saves go into the
    // project (not the copy in the build directory).
#ifdef FORGE_ASSET_DIR
    if (fs::is_directory(fs::path(FORGE_ASSET_DIR), ec)) {
        g_AssetRoot = toGeneric(fs::canonical(fs::path(FORGE_ASSET_DIR), ec));
        LOG_INFO("Asset root: %s", g_AssetRoot.c_str());
        return;
    }
#endif
    for (const fs::path& c : candidates) {
        if (fs::is_directory(c, ec)) {
            g_AssetRoot = toGeneric(fs::canonical(c, ec));
            LOG_INFO("Asset root: %s", g_AssetRoot.c_str());
            return;
        }
    }
    g_AssetRoot = toGeneric(fs::current_path(ec) / "assets");
    LOG_WARN("Could not find an 'assets' folder, defaulting to %s", g_AssetRoot.c_str());
}

const std::string& assetRoot() { return g_AssetRoot; }

std::string resolve(const std::string& assetPath) {
    if (assetPath.empty()) return g_AssetRoot;
    fs::path p(assetPath);
    if (p.is_absolute()) return assetPath;
    return toGeneric(fs::path(g_AssetRoot) / p);
}

std::string toAssetPath(const std::string& absolutePath) {
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(absolutePath), fs::path(g_AssetRoot), ec);
    if (ec || rel.empty()) return absolutePath;
    std::string s = toGeneric(rel);
    if (s.rfind("..", 0) == 0) return absolutePath;
    return s;
}

std::vector<DirEntry> listAssets(const std::string& assetDir) {
    std::vector<DirEntry> result;
    std::error_code ec;
    fs::path dir = fs::path(resolve(assetDir));
    if (!fs::is_directory(dir, ec)) return result;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        DirEntry e;
        e.name = entry.path().filename().string();
        if (e.name.empty() || e.name[0] == '.') continue;
        e.isDirectory = entry.is_directory(ec);
        e.path = assetDir.empty() ? e.name : join(assetDir, e.name);
        result.push_back(e);
    }
    std::sort(result.begin(), result.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return a.name < b.name;
    });
    return result;
}

std::vector<std::string> findAssets(const std::vector<std::string>& exts) {
    std::vector<std::string> result;
    std::error_code ec;
    fs::path root(g_AssetRoot);
    if (!fs::is_directory(root, ec)) return result;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        std::string ext = extension(it->path().string());
        if (std::find(exts.begin(), exts.end(), ext) != exts.end())
            result.push_back(toGeneric(fs::relative(it->path(), root, ec)));
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace FileSystem
} // namespace Forge
