#include "AnnotationManager.hpp"

namespace edb_next {

void AnnotationManager::setComment(Address addr, const std::string& comment) {
    if (comment.empty()) {
        comments_.erase(addr.value());
    } else {
        comments_[addr.value()] = comment;
    }
}

std::string AnnotationManager::getComment(Address addr) const {
    auto it = comments_.find(addr.value());
    if (it != comments_.end()) {
        return it->second;
    }
    return "";
}

void AnnotationManager::removeComment(Address addr) {
    comments_.erase(addr.value());
}

bool AnnotationManager::hasComment(Address addr) const {
    return comments_.find(addr.value()) != comments_.end();
}

void AnnotationManager::toggleBookmark(Address addr) {
    auto it = bookmarks_.find(addr.value());
    if (it != bookmarks_.end()) {
        bookmarks_.erase(it);
    } else {
        bookmarks_.insert(addr.value());
    }
}

void AnnotationManager::setBookmark(Address addr, bool enable) {
    if (enable) {
        bookmarks_.insert(addr.value());
    } else {
        bookmarks_.erase(addr.value());
    }
}

bool AnnotationManager::isBookmarked(Address addr) const {
    return bookmarks_.find(addr.value()) != bookmarks_.end();
}

std::vector<Address> AnnotationManager::bookmarks() const {
    std::vector<Address> result;
    result.reserve(bookmarks_.size());
    for (uint64_t val : bookmarks_) {
        result.emplace_back(val);
    }
    return result;
}

std::optional<Address> AnnotationManager::nextBookmark(Address current) const {
    auto it = bookmarks_.upper_bound(current.value());
    if (it != bookmarks_.end()) {
        return Address(*it);
    }
    if (!bookmarks_.empty()) {
        return Address(*bookmarks_.begin()); // wrap around
    }
    return std::nullopt;
}

std::optional<Address> AnnotationManager::prevBookmark(Address current) const {
    if (bookmarks_.empty()) return std::nullopt;

    auto it = bookmarks_.lower_bound(current.value());
    if (it != bookmarks_.begin()) {
        --it;
        return Address(*it);
    }
    // wrap around to the last bookmark
    return Address(*bookmarks_.rbegin());
}

void AnnotationManager::setLabel(Address addr, const std::string& label) {
    if (label.empty()) {
        labels_.erase(addr.value());
    } else {
        labels_[addr.value()] = label;
    }
}

std::string AnnotationManager::getLabel(Address addr) const {
    auto it = labels_.find(addr.value());
    if (it != labels_.end()) {
        return it->second;
    }
    return "";
}

void AnnotationManager::removeLabel(Address addr) {
    labels_.erase(addr.value());
}

bool AnnotationManager::hasLabel(Address addr) const {
    return labels_.find(addr.value()) != labels_.end();
}

std::optional<Address> AnnotationManager::findAddressByLabel(const std::string& label) const {
    if (label.empty()) return std::nullopt;
    for (const auto& [addr, lbl] : labels_) {
        if (lbl == label) {
            return Address(addr);
        }
    }
    return std::nullopt;
}

void AnnotationManager::clear() {
    comments_.clear();
    bookmarks_.clear();
    labels_.clear();
}

} // namespace edb_next
