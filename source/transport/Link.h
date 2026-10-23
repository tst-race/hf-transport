#ifndef HF_LINK_H
#define HF_LINK_H

#include <ComponentTypes.h>
#include <ITransportSdk.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <unordered_map>
#include "LinkAddress.h"
#include "HfTransportStub.h"

namespace hf_transport {

class PluginHFTransport;

class Link {
public:
    Link(const LinkID& linkId, 
         const HfLinkAddress& address, 
         const LinkProperties& properties, 
         bool isCreator, 
         PluginHFTransport* transport, 
         ITransportSdk* sdk);
    virtual ~Link();
    
    LinkID getId() const { return linkId_; }
    const LinkProperties& getProperties() const { return properties_; }
    const HfLinkAddress& getAddress() const { return address_; }
    bool isCreator() const { return isCreator_; }
    
    ComponentStatus enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content);
    ComponentStatus dequeueContent(uint64_t actionId);
    ComponentStatus fetch(std::vector<RaceHandle> handles);
    ComponentStatus post(std::vector<RaceHandle> handles, uint64_t actionId);
    void start();
    void shutdown();

protected:
    // Override these for HF-specific logic
    virtual std::string postOnActionThread(const std::string& currentHeadSha, const std::vector<RaceHandle>& handles, uint64_t actionId);
    virtual bool uploadContent(const std::vector<uint8_t>& content, const std::string& commitMessage, std::string& newCommitSha);
    virtual std::string fetchOnActionThread(const std::string& lastFetchedSha);
    virtual bool downloadNewCommits(const std::string& lastFetchedSha, std::vector<std::pair<std::string, std::vector<uint8_t>>>& newMessages);
    
    // Persistence
    void loadCursor();
    void saveCursor();
public:
    void resetCursorToInitial(const std::string& initialCommitSha);
protected:
    
    // Error mapping
    PackageStatus mapHfError(const HfError& e);
    void updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status);
    void emitEvent(const std::string& status, uint64_t actionId);
    
    ITransportSdk* sdk_;
    PluginHFTransport* transport_;
    LinkID linkId_;
    LinkProperties properties_;
    HfLinkAddress address_;
    bool isCreator_{false};
    
    // Runtime mutable state
    std::string currentHeadCommitSha_;      // Latest known commit
    std::string lastFetchedCommitSha_;      // Cursor for FETCH (persisted)
    
    struct QueuedAction { 
        bool post; 
        std::vector<RaceHandle> handles; 
        uint64_t actionId; 
    };
    
    std::thread thread_;
    std::atomic<bool> isShutdown_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<QueuedAction> actionQueue_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> contentQueue_;
    
    void runActionThread();
};

} // namespace hf_transport

#endif // HF_LINK_H