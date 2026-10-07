//
// HF Stego Transport - Transport Component
//

#ifndef HF_STEGO_TRANSPORT_PLUGIN_HF_TRANSPORT_H
#define HF_STEGO_TRANSPORT_PLUGIN_HF_TRANSPORT_H

#include <ITransportComponent.h>
#include <LinkProperties.h>
#include <ChannelProperties.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "Link.h"
#include "LinkAddress.h"
#include "HFTransportStub.h"

class PluginHFTransport : public ITransportComponent {
public:
    explicit PluginHFTransport(ITransportSdk* sdk);
    virtual ~PluginHFTransport();

    virtual ComponentStatus onUserInputReceived(RaceHandle handle, bool answered,
                                                const std::string& response) override;

    virtual TransportProperties getTransportProperties() override;
    virtual LinkProperties getLinkProperties(const LinkID& linkId) override;

    virtual ComponentStatus createLink(RaceHandle handle, const LinkID& linkId) override;
    virtual ComponentStatus loadLinkAddress(RaceHandle handle, const LinkID& linkId,
                                            const std::string& linkAddress) override;
    virtual ComponentStatus loadLinkAddresses(RaceHandle handle, const LinkID& linkId,
                                              const std::vector<std::string>& linkAddresses) override;
    virtual ComponentStatus createLinkFromAddress(RaceHandle handle, const LinkID& linkId,
                                                  const std::string& linkAddress) override;
    virtual ComponentStatus destroyLink(RaceHandle handle, const LinkID& linkId) override;

    virtual std::vector<EncodingParameters> getActionParams(const Action& action) override;
    virtual ComponentStatus enqueueContent(const EncodingParameters& params, const Action& action,
                                           const std::vector<uint8_t>& content) override;
    virtual ComponentStatus dequeueContent(const Action& action) override;
    virtual ComponentStatus doAction(const std::vector<RaceHandle>& handles, const Action& action) override;

protected:
    virtual std::shared_ptr<Link> createLinkInstance(const LinkID& linkId,
                                                     const LinkAddress& address,
                                                     const LinkProperties& properties,
                                                     LinkDirection direction);

private:
    bool preLinkCreate(const std::string& logPrefix, RaceHandle handle, const LinkID& linkId,
                       LinkSide invalidRoleLinkSide);
    ComponentStatus postLinkCreate(const std::string& logPrefix, RaceHandle handle,
                                   const LinkID& linkId, const std::shared_ptr<Link>& link,
                                   LinkStatus linkStatus);
    
    // Parse action JSON
    struct ActionJson {
        enum Type { POST, FETCH, CLEANUP, UNKNOWN } type = UNKNOWN;
        std::string linkId;
        std::string payloadHash;
    };
    std::optional<ActionJson> parseActionJson(const std::string& jsonStr);

    ITransportSdk* sdk_;
    std::string racePersona_;
    ChannelProperties channelProperties_;
    LinkProperties defaultLinkProperties_;
    
    std::shared_ptr<HFTransportStub> hfStub_;
    std::string hfUsername_;
    std::string hfToken_;
    std::string carrierModel_;
    std::string repoSuffix_;
    
    std::unordered_map<LinkID, std::shared_ptr<Link>> links_;
    std::unordered_set<LinkID> deletedLinks_;
    std::unordered_map<uint64_t, LinkID> actionToLinkIdMap_;
    std::mutex linksMutex_;
    
    std::atomic<uint64_t> nextActionId_{1};
    std::atomic<int64_t> nextAvailableHashTag_{0};
};

#endif // HF_STEGO_TRANSPORT_PLUGIN_HF_TRANSPORT_H