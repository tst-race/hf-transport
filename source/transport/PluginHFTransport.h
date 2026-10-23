#ifndef PLUGIN_HF_TRANSPORT_H
#define PLUGIN_HF_TRANSPORT_H

#include <ITransportComponent.h>
#include <ITransportSdk.h>
#include <ComponentTypes.h>
#include <string>
#include <memory>
#include "Link.h"
#include "LinkAddress.h"
#include "LinkMap.h"
#include "HfTransportStub.h"

namespace hf_transport {

class PluginHFTransport : public ITransportComponent {
public:
    explicit PluginHFTransport(ITransportSdk* sdk, const std::string& roleName);
    virtual ~PluginHFTransport();
    
    // IComponentBase
    ComponentStatus onUserInputReceived(RaceHandle handle, bool answered, const std::string& response) override;
    
    // ITransportComponent
    TransportProperties getTransportProperties() override;
    LinkProperties getLinkProperties(const LinkID& linkId) override;
    ComponentStatus createLink(RaceHandle handle, const LinkID& linkId) override;
    ComponentStatus loadLinkAddress(RaceHandle handle, const LinkID& linkId, const std::string& linkAddress) override;
    ComponentStatus loadLinkAddresses(RaceHandle handle, const LinkID& linkId, const std::vector<std::string>& linkAddresses) override;
    ComponentStatus createLinkFromAddress(RaceHandle handle, const LinkID& linkId, const std::string& linkAddress) override;
    ComponentStatus destroyLink(RaceHandle handle, const LinkID& linkId) override;
    std::vector<EncodingParameters> getActionParams(const Action& action) override;
    ComponentStatus enqueueContent(const EncodingParameters& params, const Action& action, const std::vector<uint8_t>& content) override;
    ComponentStatus dequeueContent(const Action& action) override;
    ComponentStatus doAction(const std::vector<RaceHandle>& handles, const Action& action) override;

    // Getter for HF stub (for Link access)
    HfTransportStub* getHfStub();

protected:
    virtual std::shared_ptr<Link> createLinkInstance(const LinkID& linkId, const HfLinkAddress& address, const LinkProperties& properties, bool isCreator);
    
    // Helpers (per Skyhook pattern)
    bool preLinkCreate(const std::string& logPrefix, RaceHandle handle, const LinkID& linkId, LinkSide invalidRoleLinkSide);
    ComponentStatus postLinkCreate(const std::string& logPrefix, RaceHandle handle, const LinkID& linkId, const std::shared_ptr<Link>& link, LinkStatus linkStatus);
    void handleUserInputResponse(RaceHandle handle, bool answered, const std::string& response);
    
    ComponentStatus initializeHfStub();

private:
    ITransportSdk* sdk_;
    std::string racePersona_;
    ChannelProperties channelProperties_;
    LinkProperties defaultLinkProperties_;
    LinkMap links_;
    bool ready_ = false;
    
    // User input handles
    RaceHandle hfTokenReqHandle_ = NULL_RACE_HANDLE;
    RaceHandle hfUsernameReqHandle_ = NULL_RACE_HANDLE;
    std::string hfToken_;
    std::string hfUsername_;
    
    // HF stub (initialized after credentials received)
    std::unique_ptr<HfTransportStub> hfStub_;
    
    // Generate unique repo ID
    std::string generateRepoId() const;
};

} // namespace hf_transport

#endif // PLUGIN_HF_TRANSPORT_H