//
// HF Stego Transport - Transport Component Implementation
//

#include "PluginHFTransport.h"
#include "RaceLog.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <random>
#include <cstdlib>

using nlohmann::json;

LinkProperties createDefaultLinkProperties(const ChannelProperties& channelProperties) {
    LinkProperties linkProperties;
    linkProperties.linkType = LT_BIDI;
    linkProperties.transmissionType = channelProperties.transmissionType;
    linkProperties.connectionType = channelProperties.connectionType;
    linkProperties.sendType = channelProperties.sendType;
    linkProperties.reliable = channelProperties.reliable;
    linkProperties.isFlushable = channelProperties.isFlushable;
    linkProperties.duration_s = channelProperties.duration_s;
    linkProperties.period_s = channelProperties.period_s;
    linkProperties.mtu = channelProperties.mtu;
    
    LinkPropertySet worst;
    worst.bandwidth_bps = 1000000;   // 1 Mbps worst case
    worst.latency_ms = 10000;        // 10s worst case
    worst.loss = 0.05;
    linkProperties.worst.send = worst;
    linkProperties.worst.receive = worst;
    
    linkProperties.expected = channelProperties.creatorExpected;
    
    LinkPropertySet best;
    best.bandwidth_bps = 10000000;   // 10 Mbps best case
    best.latency_ms = 2000;          // 2s best case
    best.loss = 0.001;
    linkProperties.best.send = best;
    linkProperties.best.receive = best;
    
    linkProperties.supportedHints = channelProperties.supportedHints;
    linkProperties.channelGid = channelProperties.channelGid;
    
    return linkProperties;
}

static std::string getEnvOrDefault(const char* name, const std::string& defaultValue = "") {
    const char* value = std::getenv(name);
    return value ? std::string(value) : defaultValue;
}

PluginHFTransport::PluginHFTransport(ITransportSdk* sdk)
    : sdk_(sdk),
      racePersona_(sdk->getActivePersona()),
      channelProperties_(sdk->getChannelProperties()),
      defaultLinkProperties_(createDefaultLinkProperties(channelProperties_)) {
    
    // Initialize HF stub from environment variables (non-interactive)
    hfStub_ = std::make_shared<HFTransportStub>();
    
    hfUsername_ = getEnvOrDefault("HF_USERNAME");
    hfToken_ = getEnvOrDefault("HF_TOKEN");
    carrierModel_ = getEnvOrDefault("CARRIER_MODEL", "bert-base-uncased");
    repoSuffix_ = getEnvOrDefault("REPO_SUFFIX", "test");
    
    if (!hfUsername_.empty()) {
        hfStub_->set_username(hfUsername_);
    }
    if (!hfToken_.empty()) {
        hfStub_->set_token(hfToken_);
    }
    
    RaceLog::logInfo("PluginHFTransport", "Initialized for persona: " + racePersona_, "");
    RaceLog::logInfo("PluginHFTransport", "HF Username: " + (hfUsername_.empty() ? "(not set)" : hfUsername_), "");
    RaceLog::logInfo("PluginHFTransport", "Carrier model: " + carrierModel_, "");
    RaceLog::logInfo("PluginHFTransport", "Repo suffix: " + repoSuffix_, "");
    
    // Transport is ready immediately (no user input needed)
    sdk_->updateState(COMPONENT_STATE_STARTED);
}

PluginHFTransport::~PluginHFTransport() {
    // Shutdown all links
    for (auto& [linkId, link] : links_) {
        link->shutdown();
    }
}

ComponentStatus PluginHFTransport::onUserInputReceived(RaceHandle handle, bool answered,
                                                                    const std::string& response) {
    // No user input needed - credentials come from environment variables
    return COMPONENT_OK;
}

TransportProperties PluginHFTransport::getTransportProperties() {
    return {
        {
            {"post", {"*/*"}},
            {"fetch", {"*/*"}},
        }
    };
}

LinkProperties PluginHFTransport::getLinkProperties(const LinkID& linkId) {
    std::lock_guard<std::mutex> lock(linksMutex_);
    auto it = links_.find(linkId);
    if (it != links_.end()) {
        return it->second->getProperties();
    }
    return defaultLinkProperties_;
}

bool PluginHFTransport::preLinkCreate(const std::string& logPrefix, RaceHandle handle,
                                                   const LinkID& linkId, LinkSide invalidRoleLinkSide) {
    std::lock_guard<std::mutex> lock(linksMutex_);
    
    if (channelProperties_.maxLinks > 0 && links_.size() >= channelProperties_.maxLinks) {
        RaceLog::logError(logPrefix, "Too many links: " + std::to_string(links_.size()) + 
                 ", maxLinks: " + std::to_string(channelProperties_.maxLinks), "");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return false;
    }
    
    if (channelProperties_.currentRole.linkSide == LS_UNDEF ||
        channelProperties_.currentRole.linkSide == invalidRoleLinkSide) {
        RaceLog::logError(logPrefix, "Invalid role for this call. currentRole: '" +
                 channelProperties_.currentRole.roleName + 
                 "' linkSide: " + linkSideToString(channelProperties_.currentRole.linkSide), "");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return false;
    }
    
    // Check credentials for sender role
    if (channelProperties_.currentRole.linkSide == LS_CREATOR && 
        (hfUsername_.empty() || hfToken_.empty())) {
        RaceLog::logError(logPrefix, "HF credentials not set (HF_USERNAME/HF_TOKEN env vars required for sender)", "");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return false;
    }
    
    return true;
}

ComponentStatus PluginHFTransport::postLinkCreate(const std::string& logPrefix,
                                                               RaceHandle handle,
                                                               const LinkID& linkId,
                                                               const std::shared_ptr<Link>& link,
                                                               LinkStatus linkStatus) {
    if (!link) {
        RaceLog::logError(logPrefix, "Link was null", "");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
        return COMPONENT_ERROR;
    }
    
    deletedLinks_.erase(linkId);
    links_[linkId] = link;
    sdk_->onLinkStatusChanged(handle, linkId, linkStatus, {});
    
    // Notify User Model of new link
    LinkParameters params;
    params.json = link->getAddress().toJson().dump();
    // sdk_->onTimelineUpdated(); // Not available in SDK
    
    return COMPONENT_OK;
}

std::shared_ptr<Link> PluginHFTransport::createLinkInstance(
    const LinkID& linkId, const LinkAddress& address, const LinkProperties& properties,
    LinkDirection direction) {
    
    // Map SDK LinkDirection to internal LinkDir
    LinkDir linkDir = LinkDir::SEND;
    if (direction == LD_CREATOR_TO_LOADER) {
        linkDir = LinkDir::SEND;
    } else if (direction == LD_LOADER_TO_CREATOR) {
        linkDir = LinkDir::RECEIVE;
    } else if (direction == LD_BIDI) {
        linkDir = LinkDir::BOTH;
    }
    
    auto link = std::make_shared<Link>(linkId, address, properties, linkDir, sdk_, hfStub_);
    link->start();
    return link;
}

ComponentStatus PluginHFTransport::createLink(RaceHandle handle, const LinkID& linkId) {
    std::string logPrefix = "Transport::createLink(" + linkId + "): ";
    
    RaceLog::logInfo("Transport", logPrefix + "Called, role=" + channelProperties_.currentRole.roleName + 
             " linkSide=" + std::to_string(channelProperties_.currentRole.linkSide) +
             " hfUsername_=" + (hfUsername_.empty() ? "(empty)" : hfUsername_) +
             " hfToken_=" + (hfToken_.empty() ? "(empty)" : "(set)"), "");
    
    if (!preLinkCreate(logPrefix, handle, linkId, LS_LOADER)) {
        RaceLog::logError("Transport", logPrefix + "preLinkCreate failed", "");
        return COMPONENT_ERROR;
    }
    
    // Generate repo name using deterministic suffix from environment
    // Format: transport-{repoSuffix}
    std::string repoName = "transport-" + repoSuffix_;
    
    LinkAddress address;
    address.username = hfUsername_;
    address.repo_name = repoName;
    address.carrier_model = carrierModel_;
    
    RaceLog::logInfo("Transport", logPrefix + "Creating link with address: " + address.toJson().dump(), "");
    
    LinkProperties properties = defaultLinkProperties_;
    properties.linkAddress = address.toJson().dump();
    
    // For LD_BIDI channels, create a bidirectional link that supports both send and receive
    LinkDirection linkDirection = channelProperties_.linkDirection;
    if (linkDirection == LD_UNDEF) {
        linkDirection = LD_CREATOR_TO_LOADER;  // Fallback
    }
    
    auto link = createLinkInstance(linkId, address, properties, linkDirection);
    
    return postLinkCreate(logPrefix, handle, linkId, link, LINK_CREATED);
}

ComponentStatus PluginHFTransport::loadLinkAddress(RaceHandle handle, const LinkID& linkId,
                                                                 const std::string& linkAddress) {
    std::string logPrefix = "Transport::loadLinkAddress(" + linkId + "): ";
    
    if (!preLinkCreate(logPrefix, handle, linkId, LS_CREATOR)) {
        return COMPONENT_ERROR;
    }
    
    /* Try parsing as JSON first (for SDK compatibility, like decomposed-exemplars)
     * If that fails, fall back to hf:// URL format */
    LinkAddress address;
    try {
        json j = json::parse(linkAddress);
        address = j.get<LinkAddress>();
    } catch (const json::exception&) {
        /* Not valid JSON, try hf:// URL format */
        auto addressOpt = LinkAddress::fromString(linkAddress);
        if (!addressOpt.has_value()) {
            RaceLog::logError(logPrefix, "Invalid link address format (not JSON or hf:// URL): " + linkAddress, "");
            sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
            return COMPONENT_ERROR;
        }
        address = addressOpt.value();
    }
    
    /* Verify repo exists (only if we have credentials), create if needed */
    if (!hfToken_.empty()) {
        if (!hfStub_->repo_exists(address.getRepoId())) {
            RaceLog::logInfo(logPrefix, "Repository not found, creating: " + address.repo_name, "");
            try {
                hfStub_->create_repo(address.repo_name, true);  // Create private repo (use repo_name only, not full repo_id)
            } catch (const std::exception& e) {
                std::string errorMsg = std::string(e.what());
                // Check if error is "already exists" (HTTP 409) - treat as success
                if (errorMsg.find("already") != std::string::npos || errorMsg.find("409") != std::string::npos) {
                    RaceLog::logInfo(logPrefix, "Repository already exists, continuing: " + address.repo_name, "");
                } else {
                    RaceLog::logError(logPrefix, "Failed to create repository: " + errorMsg, "");
                    sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
                    return COMPONENT_ERROR;
                }
            }
        }
    } else {
        // No credentials, just check if repo exists (public repos)
        if (!hfStub_->repo_exists(address.getRepoId())) {
            RaceLog::logError(logPrefix, "Repository not found and no credentials to create: " + address.getRepoId(), "");
            sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
            return COMPONENT_ERROR;
        }
    }
    
    LinkProperties properties = defaultLinkProperties_;
    properties.linkAddress = address.toJson().dump();
    
    auto link = createLinkInstance(linkId, address, properties, LD_LOADER_TO_CREATOR);
    
    return postLinkCreate(logPrefix, handle, linkId, link, LINK_LOADED);
}

ComponentStatus PluginHFTransport::loadLinkAddresses(
    RaceHandle handle, const LinkID& linkId, const std::vector<std::string>& linkAddresses) {
    
    RaceLog::logError("Transport::loadLinkAddresses", "Multi-address loading not supported", "");
    sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
    return COMPONENT_ERROR;
}

ComponentStatus PluginHFTransport::createLinkFromAddress(RaceHandle handle, const LinkID& linkId,
                                                                      const std::string& linkAddress) {
    // Same as loadLinkAddress for this transport
    return loadLinkAddress(handle, linkId, linkAddress);
}

ComponentStatus PluginHFTransport::destroyLink(RaceHandle handle, const LinkID& linkId) {
    std::string logPrefix = "Transport::destroyLink(" + linkId + "): ";
    
    std::lock_guard<std::mutex> lock(linksMutex_);
    auto it = links_.find(linkId);
    if (it == links_.end()) {
        RaceLog::logError(logPrefix, "Link not found", "");
        return COMPONENT_ERROR;
    }
    
    auto link = it->second;
    links_.erase(it);
    deletedLinks_.insert(linkId);
    
    link->shutdown();
    sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, {});
    
    // Notify User Model - onTimelineUpdated not available in SDK
    
    return COMPONENT_OK;
}

std::optional<PluginHFTransport::ActionJson> 
PluginHFTransport::parseActionJson(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        ActionJson action;
        action.linkId = j.value("linkId", "");
        action.payloadHash = j.value("payloadHash", "");
        
        std::string typeStr = j.value("type", "");
        if (typeStr == "post") action.type = ActionJson::POST;
        else if (typeStr == "fetch") action.type = ActionJson::FETCH;
        else if (typeStr == "cleanup") action.type = ActionJson::CLEANUP;
        else action.type = ActionJson::UNKNOWN;
        
        return action;
    } catch (const std::exception& e) {
        RaceLog::logError("Transport", "Failed to parse action JSON: " + std::string(e.what()), "");
        return std::nullopt;
    }
}

std::vector<EncodingParameters> PluginHFTransport::getActionParams(const Action& action) {
    auto actionJson = parseActionJson(action.json);
    if (!actionJson.has_value()) {
        return {};
    }
    
    std::vector<EncodingParameters> params;
    
    switch (actionJson->type) {
        case ActionJson::POST:
            params.push_back({
                actionJson->linkId,
                "text/plain",  // Base64 encoding outputs text/plain
                true,
                json{
                    {"carrier_model", carrierModel_},
                    {"action_type", "post"},
                    {"linkId", actionJson->linkId}
                }.dump()
            });
            break;
            
        case ActionJson::FETCH:
            params.push_back({
                actionJson->linkId,
                "text/plain",  // Base64 encoding outputs text/plain
                false,
                json{
                    {"action_type", "fetch"},
                    {"linkId", actionJson->linkId}
                }.dump()
            });
            break;
            
        case ActionJson::CLEANUP:
            params.push_back({
                actionJson->linkId,
                "text/plain",  // Use text/plain for cleanup too
                false,
                json{
                    {"action_type", "cleanup"},
                    {"linkId", actionJson->linkId}
                }.dump()
            });
            break;
            
        default:
            break;
    }
    
    return params;
}

ComponentStatus PluginHFTransport::enqueueContent(const EncodingParameters& params,
                                                                const Action& action,
                                                                const std::vector<uint8_t>& content) {
    std::lock_guard<std::mutex> lock(linksMutex_);
    
    // Determine linkId: use params.linkId, or extract from action.json if empty
    LinkID linkId = params.linkId;
    if (linkId.empty()) {
        auto actionJson = parseActionJson(action.json);
        if (actionJson && !actionJson->linkId.empty()) {
            linkId = actionJson->linkId;
            RaceLog::logDebug("Transport", "Using linkId from action.json: " + linkId, "");
        }
    }
    
    // Map action ID to link ID for later retrieval
    actionToLinkIdMap_[action.actionId] = linkId;
    
    // Handle wildcard linkId "*" - apply to all links
    if (linkId == "*") {
        if (links_.empty()) {
            RaceLog::logError("Transport", "No links available for wildcard enqueue", "");
            return COMPONENT_ERROR;
        }
        for (auto& [lid, link] : links_) {
            if (deletedLinks_.count(lid) != 0) continue;
            link->enqueueContent(action.actionId, content);
            auto actionJson = parseActionJson(action.json);
            if (actionJson && actionJson->type == ActionJson::POST) {
                link->setPendingPayload(content);
            }
        }
        return COMPONENT_OK;
    }
    
    // Check if link was deleted
    if (deletedLinks_.count(linkId) != 0) {
        RaceLog::logDebug("Transport", "Ignoring enqueue for deleted link " + linkId, "");
        return COMPONENT_OK;
    }
    
    auto it = links_.find(linkId);
    if (it == links_.end()) {
        RaceLog::logError("Transport", "Link not found for enqueue: " + linkId, "");
        return COMPONENT_ERROR;
    }
    
    // Store content for this action
    it->second->enqueueContent(action.actionId, content);
    
    // For POST actions, also store as pending payload
    auto actionJson = parseActionJson(action.json);
    if (actionJson && actionJson->type == ActionJson::POST) {
        it->second->setPendingPayload(content);
    }
    
    return COMPONENT_OK;
}

ComponentStatus PluginHFTransport::dequeueContent(const Action& action) {
    std::lock_guard<std::mutex> lock(linksMutex_);
    
    // Find link ID for this action
    LinkID linkId;
    auto actionJson = parseActionJson(action.json);
    if (actionJson && !actionJson->linkId.empty()) {
        linkId = actionJson->linkId;
    } else {
        auto it = actionToLinkIdMap_.find(action.actionId);
        if (it != actionToLinkIdMap_.end()) {
            linkId = it->second;
        } else {
            return COMPONENT_OK; // No link mapping, nothing to dequeue
        }
    }
    
    // Handle wildcard linkId "*" - apply to all links
    if (linkId == "*") {
        for (auto& [lid, link] : links_) {
            if (deletedLinks_.count(lid) != 0) continue;
            link->dequeueContent(action.actionId);
        }
        actionToLinkIdMap_.erase(action.actionId);
        return COMPONENT_OK;
    }
    
    if (deletedLinks_.count(linkId) != 0) {
        actionToLinkIdMap_.erase(action.actionId);
        return COMPONENT_OK;
    }
    
    auto it = links_.find(linkId);
    if (it != links_.end()) {
        it->second->dequeueContent(action.actionId);
    }
    
    actionToLinkIdMap_.erase(action.actionId);
    return COMPONENT_OK;
}

ComponentStatus PluginHFTransport::doAction(const std::vector<RaceHandle>& handles,
                                                          const Action& action) {
    auto actionJson = parseActionJson(action.json);
    if (!actionJson.has_value()) {
        RaceLog::logError("Transport", "Invalid action JSON: " + action.json, "");
        for (auto handle : handles) {
            sdk_->onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC);
        }
        return COMPONENT_ERROR;
    }
    
    LinkID linkId = actionJson->linkId;
    
    std::lock_guard<std::mutex> lock(linksMutex_);
    
    // Handle wildcard linkId "*" - apply to all links
    if (linkId == "*") {
        if (links_.empty()) {
            RaceLog::logError("Transport", "No links available for wildcard action", "");
            for (auto handle : handles) {
                sdk_->onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC);
            }
            return COMPONENT_ERROR;
        }
        ComponentStatus overallStatus = COMPONENT_OK;
        for (auto& [lid, link] : links_) {
            if (deletedLinks_.count(lid) != 0) continue;
            ComponentStatus status;
            switch (actionJson->type) {
                case ActionJson::POST:
                    status = link->post(handles, action.actionId);
                    break;
                case ActionJson::FETCH:
                    status = link->fetch();
                    break;
                case ActionJson::CLEANUP:
                    status = link->cleanup();
                    break;
                default:
                    RaceLog::logError("Transport", "Unknown action type", "");
                    status = COMPONENT_ERROR;
                    break;
            }
            if (status != COMPONENT_OK) {
                overallStatus = COMPONENT_ERROR;
            }
        }
        return overallStatus;
    }
    
    auto it = links_.find(linkId);
    if (it == links_.end()) {
        if (deletedLinks_.count(linkId) != 0) {
            RaceLog::logWarning("Transport", "Action for deleted link " + linkId, "");
            for (auto handle : handles) {
                sdk_->onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC);
            }
            return COMPONENT_OK;
        }
        RaceLog::logError("Transport", "Link not found: " + linkId, "");
        for (auto handle : handles) {
            sdk_->onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC);
        }
        return COMPONENT_ERROR;
    }
    
    auto link = it->second;
    
    switch (actionJson->type) {
        case ActionJson::POST:
            return link->post(handles, action.actionId);
            
        case ActionJson::FETCH:
            return link->fetch();
            
        case ActionJson::CLEANUP:
            return link->cleanup();
            
        default:
            RaceLog::logError("Transport", "Unknown action type", "");
            for (auto handle : handles) {
                sdk_->onPackageStatusChanged(handle, PACKAGE_FAILED_GENERIC);
            }
            return COMPONENT_ERROR;
    }
}

#ifndef TESTBUILD
ITransportComponent *createTransport(const std::string &transport, ITransportSdk *sdk,
                                     const std::string &roleName,
                                     const PluginConfig &pluginConfig) {
    TRACE_FUNCTION(transport, roleName, pluginConfig.pluginDirectory);
    return new PluginHFTransport(sdk);
}
void destroyTransport(ITransportComponent *component) {
    TRACE_FUNCTION();
    delete component;
}

const RaceVersionInfo raceVersion = RACE_VERSION;
#endif