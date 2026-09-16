#include "SourceFileManager.hpp"
#include <fstream>
#include <unistd.h>
#include <filesystem>

namespace edb_next {

SourceFileManager& SourceFileManager::instance() {
    static SourceFileManager inst;
    return inst;
}

void SourceFileManager::addSearchPath(const std::string& path) {
    if (path.empty()) return;
    for (const auto& p : searchPaths_) {
        if (p == path) return;
    }
    searchPaths_.push_back(path);
}

void SourceFileManager::clear() {
    fileLinesCache_.clear();
    resolvedPathCache_.clear();
}

std::string SourceFileManager::resolvePath(const std::string& rawPath) {
    if (rawPath.empty()) return "";

    auto it = resolvedPathCache_.find(rawPath);
    if (it != resolvedPathCache_.end()) {
        return it->second;
    }

    // 1. Direct path check
    if (access(rawPath.c_str(), R_OK) == 0) {
        resolvedPathCache_[rawPath] = rawPath;
        return rawPath;
    }

    // 2. Extract filename
    std::string filename = rawPath;
    auto last_slash = rawPath.find_last_of('/');
    if (last_slash != std::string::npos) {
        filename = rawPath.substr(last_slash + 1);
    }

    // 3. Search in configured search paths
    for (const auto& searchDir : searchPaths_) {
        std::filesystem::path p1 = std::filesystem::path(searchDir) / rawPath;
        if (access(p1.c_str(), R_OK) == 0) {
            resolvedPathCache_[rawPath] = p1.string();
            return p1.string();
        }

        std::filesystem::path p2 = std::filesystem::path(searchDir) / filename;
        if (access(p2.c_str(), R_OK) == 0) {
            resolvedPathCache_[rawPath] = p2.string();
            return p2.string();
        }
    }

    // 4. Fallback to rawPath
    resolvedPathCache_[rawPath] = rawPath;
    return rawPath;
}

const std::vector<std::string>& SourceFileManager::getFileLines(const std::string& filePath) {
    std::string resolved = resolvePath(filePath);
    if (resolved.empty()) return emptyLines_;

    auto it = fileLinesCache_.find(resolved);
    if (it != fileLinesCache_.end()) {
        return it->second;
    }

    std::ifstream file(resolved);
    if (!file.is_open()) {
        return emptyLines_;
    }

    std::vector<std::string> lines;
    lines.reserve(256);
    lines.push_back(""); // 1-based indexing: index 0 is dummy

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }

    fileLinesCache_[resolved] = std::move(lines);
    return fileLinesCache_[resolved];
}

std::string SourceFileManager::getLineText(const std::string& filePath, int line) {
    if (line <= 0) return "";
    const auto& lines = getFileLines(filePath);
    if (static_cast<size_t>(line) < lines.size()) {
        return lines[line];
    }
    return "";
}

size_t SourceFileManager::lineCount(const std::string& filePath) {
    const auto& lines = getFileLines(filePath);
    return lines.empty() ? 0 : (lines.size() - 1);
}

} // namespace edb_next
