#include "LinkMap.h"

namespace hf_transport {

std::shared_ptr<Link> LinkMap::get(const LinkID& linkId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = links_.find(linkId);
    return (it != links_.end()) ? it->second : nullptr;
}

void LinkMap::put(const LinkID& linkId, std::shared_ptr<Link> link) {
    std::lock_guard<std::mutex> lock(mutex_);
    links_[linkId] = std::move(link);
}

void LinkMap::remove(const LinkID& linkId) {
    std::lock_guard<std::mutex> lock(mutex_);
    links_.erase(linkId);
}

bool LinkMap::contains(const LinkID& linkId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return links_.find(linkId) != links_.end();
}

std::vector<LinkID> LinkMap::getAllIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LinkID> ids;
    ids.reserve(links_.size());
    for (const auto& pair : links_) {
        ids.push_back(pair.first);
    }
    return ids;
}

size_t LinkMap::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return links_.size();
}

} // namespace hf_transport