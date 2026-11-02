#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

// Like ProjectGame.txt, this project setting travels with Content, including exports.
namespace ProjectHud {
inline std::filesystem::path Resolve(const std::filesystem::path& content, const std::string& selected) {
    if (selected.empty()) return {};
    const auto relative = std::filesystem::u8path(selected);
    if (relative.is_absolute() || relative.has_root_name() || relative.extension() != ".ui")
        throw std::runtime_error("HUD: expected a Content-relative .ui path");
    const auto root = std::filesystem::weakly_canonical(content);
    const auto file = std::filesystem::weakly_canonical(root / relative);
    const auto local = file.lexically_relative(root);
    if (local.empty() || *local.begin() == "..") throw std::runtime_error("HUD: path escapes Content");
    return file;
}
inline std::filesystem::path Read(const std::filesystem::path& content) {
    std::ifstream file(content / "ProjectHud.txt", std::ios::binary);
    std::string selected;
    std::getline(file, selected);
    if (!selected.empty() && selected.back() == '\r') selected.pop_back();
    return Resolve(content, selected);
}
}
