//
// HF Stego Transport - Link Instance
//

#ifndef HF_STEGO_TRANSPORT_LINK_H
#define HF_STEGO_TRANSPORT_LINK_H

#include <ComponentTypes.h>
#include <LinkProperties.h>
#include <PackageStatus.h>
#include <SdkResponse.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "LinkAddress.h"
#include "HFTransportStub.h"  // Our Phase 3 stub wrapper

class ITransportSdk;

enum class LinkDir {
    SEND,      // Sender: creates repo, uploads, deletes
    RECEIVE,   // Receiver: downloads, decodes
    BOTH       // Both directions (two separate links)
};

struct LinkState {
    LinkID linkId;
    LinkAddress address;
    LinkDir direction;
    std::atomic<bool> active{true};
    std::string lastCommitSha;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point lastActivity;
    int fetchAttempts{0};
    int maxFetchAttempts{120};
    
    // Content queue for pending actions
    std::mutex contentMutex;
    std::unordered_map<uint64_t, std::vector<uint8_t>> contentQueue;
    
    // Pending outgoing payload
    std::vector<uint8_t> pendingPayload;
    bool hasPendingPayload{false};
};

/**
 * @brief A single link instance for HF Stego Transport
 */
class Link {
public:
    Link(LinkID linkId, LinkAddress address, LinkProperties properties, 
         LinkDir direction, ITransportSdk* sdk,
         std::shared_ptr<HFTransportStub> stub);
    
    virtual ~Link();
    
    LinkID getId() const { return linkId_; }
    const LinkProperties& getProperties() const { return properties_; }
    LinkDir getDirection() const { return direction_; }
    bool isActive() const { return state_->active.load(); }
    const LinkAddress& getAddress() const { return address_; }
    
    // Content management
    ComponentStatus enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content);
    ComponentStatus dequeueContent(uint64_t actionId);
    std::optional<std::vector<uint8_t>> getContent(uint64_t actionId);
    
    // Set pending payload for POST action
    void setPendingPayload(const std::vector<uint8_t>& payload);
    std::vector<uint8_t> takePendingPayload();
    
    // Link operations
    ComponentStatus post(std::vector<RaceHandle> handles, uint64_t actionId);
    ComponentStatus fetch();
    ComponentStatus cleanup();
    
    // Lifecycle
    void start();
    void shutdown();
    
    // Update activity timestamp
    void touchActivity();

private:
    ITransportSdk* sdk_;
    std::shared_ptr<HFTransportStub> stub_;
    LinkID linkId_;
    LinkAddress address_;
    LinkProperties properties_;
    LinkDir direction_;
    std::shared_ptr<LinkState> state_;
    
    std::thread thread_;
    std::atomic<bool> isShutdown_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> actionQueue_;
    
    void runActionThread();
    void updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status);
    
    // Internal operations
    ComponentStatus doPost(uint64_t actionId);
    ComponentStatus doFetch();
    ComponentStatus doCleanup();
    
    // HF Hub operations
    ComponentStatus uploadModel(const std::vector<uint8_t>& modelData);
    ComponentStatus downloadModel(std::vector<uint8_t>& modelData);
    ComponentStatus deleteRepo();
};

#endif // HF_STEGO_TRANSPORT_LINK_H