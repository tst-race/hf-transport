#ifndef LINK_MAP_H
#define LINK_MAP_H

#include <LinkProperties.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>

namespace hf_transport {

class Link;

class LinkMap {
public:
    std::shared_ptr<Link> get(const LinkID& linkId);
    void put(const LinkID& linkId, std::shared_ptr<Link> link);
    void remove(const LinkID& linkId);
    bool contains(const LinkID& linkId) const;
    std::vector<LinkID> getAllIds() const;
    size_t size() const;

private:
    std::unordered_map<LinkID, std::shared_ptr<Link>> links_;
    mutable std::mutex mutex_;
};

} // namespace hf_transport

#endif // LINK_MAP_H