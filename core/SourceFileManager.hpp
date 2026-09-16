#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace edb_next {

class SourceFileManager {
public:
    static SourceFileManager& instance();

    SourceFileManager() = default;
    ~SourceFileManager() = default;

    // Retrieve all lines for a file (1-indexed vector: index 0 is empty dummy line)
    const std::vector<std::string>& getFileLines(const std::string& filePath);

    // Retrieve a single line of text from file (1-based line number)
    std::string getLineText(const std::string& filePath, int line);

    // Total line count in file
    size_t lineCount(const std::string& filePath);

    // Add source search directory (e.g. project workspace root)
    void addSearchPath(const std::string& path);

    // Clear cached files
    void clear();

    // Resolve an actual existing filesystem path from a DWARF-provided path
    std::string resolvePath(const std::string& rawPath);

private:
    std::vector<std::string> searchPaths_;
    std::unordered_map<std::string, std::vector<std::string>> fileLinesCache_;
    std::unordered_map<std::string, std::string> resolvedPathCache_;
    const std::vector<std::string> emptyLines_;
};

} // namespace edb_next
