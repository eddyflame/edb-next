#pragma once

#include "Types.hpp"
#include <string>
#include <unordered_map>
#include <set>
#include <vector>
#include <optional>

namespace edb_next {

class AnnotationManager {
public:
    AnnotationManager() = default;

    // Comments
    void setComment(Address addr, const std::string& comment);
    [[nodiscard]] std::string getComment(Address addr) const;
    void removeComment(Address addr);
    [[nodiscard]] bool hasComment(Address addr) const;
    [[nodiscard]] const std::unordered_map<uint64_t, std::string>& allComments() const noexcept { return comments_; }

    // Bookmarks
    void toggleBookmark(Address addr);
    void setBookmark(Address addr, bool enable);
    [[nodiscard]] bool isBookmarked(Address addr) const;
    [[nodiscard]] std::vector<Address> bookmarks() const;
    [[nodiscard]] std::optional<Address> nextBookmark(Address current) const;
    [[nodiscard]] std::optional<Address> prevBookmark(Address current) const;

    // Labels
    void setLabel(Address addr, const std::string& label);
    [[nodiscard]] std::string getLabel(Address addr) const;
    void removeLabel(Address addr);
    [[nodiscard]] bool hasLabel(Address addr) const;
    [[nodiscard]] const std::unordered_map<uint64_t, std::string>& allLabels() const noexcept { return labels_; }
    [[nodiscard]] std::optional<Address> findAddressByLabel(const std::string& label) const;

    void clear();

private:
    std::unordered_map<uint64_t, std::string> comments_;
    std::set<uint64_t> bookmarks_;
    std::unordered_map<uint64_t, std::string> labels_;
};

} // namespace edb_next
