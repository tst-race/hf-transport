#include "PluginHFTransport.h"
#include "log.h"
#include "JsonTypes.h"
#include "PersistentStorageHelpers.h"
#include <ITransportSdk.h>
#include <ComponentTypes.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace hf_transport {

using json = nlohmann::json;

PluginHFTransport::PluginHFTransport(ITransportSdk* sdk, const std::string& /*roleName*/)
    : sdk_(sdk), racePersona_(sdk->getActivePersona()) {
    TRACE_METHOD();
    
    channelProperties_ = sdk_->getChannelProperties();
    
    // Initialize default link properties from channel properties
    defaultLinkProperties_.linkType = LT_BIDI;
    defaultLinkProperties_.connectionType = CT_INDIRECT;
    defaultLinkProperties_.transmissionType = TT_UNICAST;
    defaultLinkProperties_.connectionType = CT_INDIRECT;
    defaultLinkProperties_.sendType = ST_STORED_ASYNC;
    defaultLinkProperties_.reliable = true;
    defaultLinkProperties_.isFlushable = false;
    defaultLinkProperties_.duration_s = 86400;
    defaultLinkProperties_.period_s = 3600;
    defaultLinkProperties_.mtu = 52428800;  // 50MB
    defaultLinkProperties_.channelGid = "hfTransportComposition";
    defaultLinkProperties_.supportedHints = {"polling_interval_ms"};
    
    // Try to get credentials from environment variables first (for automated tests)
    const char* envToken = std::getenv("HF_TOKEN");
    const char* envUsername = std::getenv("HF_USERNAME");
    
    if (envToken && envToken[0] != '\0') {
        hfToken_ = envToken;
        logInfo("Using HF token from environment variable");
    } else {
        // Request user input for HF token
        hfTokenReqHandle_ = sdk_->requestPluginUserInput("hf_token", "HF Write Token (write scope PAT)", true).handle;
    }
    
    if (envUsername && envUsername[0] != '\0') {
        hfUsername_ = envUsername;
        logInfo("Using HF username from environment variable");
    } else {
        // Request user input for HF username
        hfUsernameReqHandle_ = sdk_->requestPluginUserInput("hf_username", "HF Username", false).handle;
    }
    
    // If both credentials are available from environment, mark as ready
    fprintf(stderr, "Constructor: hfToken_=%s, hfUsername_=%s\n", hfToken_.empty() ? "empty" : "set", hfUsername_.empty() ? "empty" : "set");
    fflush(stderr);
    if (!hfToken_.empty() && !hfUsername_.empty()) {
        fprintf(stderr, "Constructor: calling initializeHfStub\n");
        fflush(stderr);
        if (initializeHfStub() == COMPONENT_OK) {
            ready_ = true;
            sdk_->updateState(COMPONENT_STATE_STARTED);
            logInfo("PluginHFTransport ready with credentials from environment");
            fprintf(stderr, "Constructor: initializeHfStub returned OK\n");
            fflush(stderr);
        } else {
            sdk_->updateState(COMPONENT_STATE_FAILED);
            logError("PluginHFTransport failed: could not initialize HF stub");
            fprintf(stderr, "Constructor: initializeHfStub returned ERROR\n");
            fflush(stderr);
        }
    }
    
    logInfo("PluginHFTransport constructed for persona: " + racePersona_);
}

PluginHFTransport::~PluginHFTransport() {
    TRACE_METHOD();
    // Shutdown all links
    for (const auto& linkId : links_.getAllIds()) {
        auto link = links_.get(linkId);
        if (link) link->shutdown();
    }
}

ComponentStatus PluginHFTransport::onUserInputReceived(RaceHandle handle, bool answered, const std::string& response) {
    TRACE_METHOD();
    handleUserInputResponse(handle, answered, response);
    return COMPONENT_OK;
}

void PluginHFTransport::handleUserInputResponse(RaceHandle handle, bool answered, const std::string& response) {
    if (handle == hfTokenReqHandle_) {
        hfTokenReqHandle_ = NULL_RACE_HANDLE;
        if (answered && !response.empty()) {
            hfToken_ = response;
            logInfo("Received HF token");
        } else {
            logError("HF token is required but not provided");
        }
    } else if (handle == hfUsernameReqHandle_) {
        hfUsernameReqHandle_ = NULL_RACE_HANDLE;
        if (answered && !response.empty()) {
            hfUsername_ = response;
            logInfo("Received HF username: " + hfUsername_);
        } else {
            logWarning("HF username not provided, will use token default");
        }
    }
    
    // Check if all required inputs received
    if (hfTokenReqHandle_ == NULL_RACE_HANDLE && hfUsernameReqHandle_ == NULL_RACE_HANDLE) {
        if (!hfToken_.empty()) {
            if (initializeHfStub() == COMPONENT_OK) {
                ready_ = true;
                sdk_->updateState(COMPONENT_STATE_STARTED);
                logInfo("Transport component ready");
            }
        } else {
            sdk_->updateState(COMPONENT_STATE_FAILED);
            logError("Transport component failed: missing HF token");
        }
    }
}

ComponentStatus PluginHFTransport::initializeHfStub() {
    TRACE_METHOD();
    fprintf(stderr, "initializeHfStub: ENTRY\n");
    fflush(stderr);
    
    try {
        fprintf(stderr, "initializeHfStub: creating HfTransportStub\n");
        fflush(stderr);
        hfStub_ = std::make_unique<HfTransportStub>(hfToken_, hfUsername_);
        fprintf(stderr, "initializeHfStub: HfTransportStub created\n");
        fflush(stderr);
        logInfo("HF Transport Stub initialized");
        fprintf(stderr, "initializeHfStub: logInfo called\n");
        fflush(stderr);
        return COMPONENT_OK;
    } catch (const std::exception& e) {
        logError("Failed to initialize HF stub: " + std::string(e.what()));
        fprintf(stderr, "initializeHfStub: exception: %s\n", e.what());
        fflush(stderr);
        return COMPONENT_ERROR;
    }
}

HfTransportStub* PluginHFTransport::getHfStub() {
    return hfStub_.get();
}

std::string PluginHFTransport::generateRepoId() const {
    boost::uuids::random_generator gen;
    boost::uuids::uuid uuid = gen();
    return "transport-" + boost::uuids::to_string(uuid);
}

TransportProperties PluginHFTransport::getTransportProperties() {
    TRACE_METHOD();
    
    TransportProperties props;
    // We support raw binary (noop encoding)
    props.supportedActions["post"] = {"application/octet-stream"};
    props.supportedActions["fetch"] = {"application/octet-stream"};
    return props;
}

LinkProperties PluginHFTransport::getLinkProperties(const LinkID& linkId) {
    TRACE_METHOD();
    
    LinkProperties props = defaultLinkProperties_;
    
    auto link = links_.get(linkId);
    if (link) {
        props.linkAddress = link->getAddress().toString();
    }
    
    return props;
}

bool PluginHFTransport::preLinkCreate(const std::string& logPrefixParam, RaceHandle handle, const LinkID& linkId, LinkSide invalidRoleLinkSide) {
    TRACE_METHOD();
    
    if (!ready_) {
        logError(logPrefixParam + "Component not ready");
        return false;
    }
    
    if (channelProperties_.currentRole.linkSide == invalidRoleLinkSide) {
        logError(logPrefixParam + "Invalid role for link creation");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, LinkParameters());
        return false;
    }
    
    if (links_.size() >= static_cast<size_t>(channelProperties_.maxLinks)) {
        logError(logPrefixParam + "Max links exceeded");
        sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, LinkParameters());
        return false;
    }
    
    if (links_.contains(linkId)) {
        logError(logPrefixParam + "Link already exists: " + linkId);
        return false;
    }
    
    return true;
}

ComponentStatus PluginHFTransport::postLinkCreate(const std::string& logPrefixParam, RaceHandle handle, const LinkID& linkId, const std::shared_ptr<Link>& link, LinkStatus linkStatus) {
    TRACE_METHOD();
    
    links_.put(linkId, link);
    link->start();
    
    LinkParameters params;
    json paramsJson;
    paramsJson["linkId"] = linkId;
    paramsJson["linkAddress"] = link->getAddress().toString();
    params.json = paramsJson.dump();
    
    sdk_->onLinkStatusChanged(handle, linkId, linkStatus, params);
    logInfo(logPrefixParam + "Link " + (linkStatus == LINK_CREATED ? "created" : "loaded") + ": " + linkId);
    
    return COMPONENT_OK;
}

ComponentStatus PluginHFTransport::createLink(RaceHandle handle, const LinkID& linkId) {
    std::string logPrefixParam = "createLink(" + linkId + "): ";
    TRACE_METHOD();
    
    if (!preLinkCreate(logPrefixParam, handle, linkId, LS_LOADER)) {
        return COMPONENT_ERROR;
    }
    
    // Sender side: create new repo
    std::string repoId = generateRepoId();
    std::string initialCommitSha = "main";  // Baseline before any uploads
    
    try {
        // Create repo via HF stub
        auto repoInfo = hfStub_->create_repo(repoId, false, false);
        initialCommitSha = repoInfo.commitSha.empty() ? "main" : repoInfo.commitSha;
        
        // Create initial commit with placeholder file (uses commit API, not large file upload)
        initialCommitSha = hfStub_->create_initial_commit(hfUsername_ + "/" + repoId, "Initial commit");
        
    } catch (const HfError& e) {
        logError("Failed to create repo: " + std::string(e.what()));
        // Don't notify LINK_DESTROYED - link was never created
        return COMPONENT_ERROR;
    } catch (const std::exception& e) {
        logError("Failed to create repo (std::exception): " + std::string(e.what()));
        return COMPONENT_ERROR;
    } catch (...) {
        logError("Failed to create repo (unknown exception)");
        return COMPONENT_ERROR;
    }
    
    // Create link address
    HfLinkAddress address(hfUsername_, repoId, initialCommitSha, "main");
    
    // Create link instance (isCreator = true for sender)
    auto link = createLinkInstance(linkId, address, defaultLinkProperties_, true);
    
    // Persist address for reload
    psh::saveValue(sdk_, psh::addressKey(linkId), address.toString());
    
    return postLinkCreate(logPrefixParam, handle, linkId, link, LINK_CREATED);
}

ComponentStatus PluginHFTransport::loadLinkAddress(RaceHandle handle, const LinkID& linkId, const std::string& linkAddress) {
    std::string logPrefixParam = "loadLinkAddress(" + linkId + "): ";
    TRACE_METHOD();
    
    fprintf(stderr, "loadLinkAddress: ENTRY, linkId=%s\n", linkId.c_str());
    fflush(stderr);
    
    if (!preLinkCreate(logPrefixParam, handle, linkId, LS_CREATOR)) {
        fprintf(stderr, "loadLinkAddress: preLinkCreate returned false\n");
        fflush(stderr);
        return COMPONENT_ERROR;
    }
    
    fprintf(stderr, "loadLinkAddress: preLinkCreate OK, hfStub_=%p\n", reinterpret_cast<void*>(hfStub_.get()));
    fflush(stderr);
    
    // Ensure HF stub is initialized
    logDebug(logPrefixParam + "hfStub_ is " + std::string(hfStub_ ? "valid" : "null"));
    if (!hfStub_) {
        logDebug(logPrefixParam + "Calling initializeHfStub");
        fprintf(stderr, "loadLinkAddress: hfStub_ is null, calling initializeHfStub\n");
        fflush(stderr);
        if (initializeHfStub() != COMPONENT_OK) {
            logError(logPrefixParam + "Failed to initialize HF stub");
            fprintf(stderr, "loadLinkAddress: initializeHfStub failed\n");
            fflush(stderr);
            return COMPONENT_ERROR;
        }
        logDebug(logPrefixParam + "initializeHfStub returned OK, hfStub_ is " + std::string(hfStub_ ? "valid" : "null"));
        fprintf(stderr, "loadLinkAddress: initializeHfStub OK, hfStub_=%p\n", reinterpret_cast<void*>(hfStub_.get()));
        fflush(stderr);
    }
    
    // Parse address (supports both JSON and hf:// formats)
    HfLinkAddress address = HfLinkAddress::parse(linkAddress);
    // For loader role, allow empty initialCommitSha (will fetch latest on first poll)
    // For creator role, initialCommitSha is required (validated in createLinkFromAddress)
    // loadLinkAddress is always the loader role (loads peer's repo)
    bool isLoader = true;
    if (!address.isValid() && !(isLoader && address.initialCommitSha.empty() && !address.username.empty() && !address.repoId.empty())) {
        logError(logPrefixParam + "Invalid link address format");
        // Don't notify LINK_DESTROYED - link was never created
        return COMPONENT_ERROR;
    }
    
    // For CREATOR_TO_LOADER: receiver loads sender's repo address.
    // The repo may not exist yet (sender hasn't created it). 
    // Don't block here - create the link and let fetch retry gracefully on next poll.
    // This makes the transport robust to timing conditions where receiver starts 
    // polling before sender has created the link.
    logInfo(logPrefixParam + "Loading link address for repo " + address.repoId + " (will retry on fetch if repo not yet created)");
    
    // Create link instance (isCreator = false for receiver)
    auto link = createLinkInstance(linkId, address, defaultLinkProperties_, false);
    
    // Persist address for reload
    psh::saveValue(sdk_, psh::addressKey(linkId), linkAddress);
    
    return postLinkCreate(logPrefixParam, handle, linkId, link, LINK_LOADED);
}

ComponentStatus PluginHFTransport::loadLinkAddresses(RaceHandle /*handle*/, const LinkID& /*linkId*/, const std::vector<std::string>& /*linkAddresses*/) {
    TRACE_METHOD();
    // Not supported for HF transport (single address per link)
    return COMPONENT_ERROR;
}

ComponentStatus PluginHFTransport::createLinkFromAddress(RaceHandle handle, const LinkID& linkId, const std::string& linkAddress) {
    std::string logPrefixParam = "createLinkFromAddress(" + linkId + "): ";
    TRACE_METHOD();
    
    if (!preLinkCreate(logPrefixParam, handle, linkId, LS_LOADER)) {
        return COMPONENT_ERROR;
    }
    
    // Ensure HF stub is initialized
    if (!hfStub_) {
        if (initializeHfStub() != COMPONENT_OK) {
            logError(logPrefixParam + "Failed to initialize HF stub");
            return COMPONENT_ERROR;
        }
    }
    
    // Parse address (supports both JSON and hf:// formats)
    HfLinkAddress address = HfLinkAddress::parse(linkAddress);
    bool reusedRepo = false;
    
    // For creator role, allow empty initialCommitSha (will be populated after repo creation)
    // For loader role, initialCommitSha is required to know where to start fetching
    // createLinkFromAddress is always the creator role (creates repo)
    bool isCreator = true;
    if (!address.isValid() && !(isCreator && address.initialCommitSha.empty() && !address.username.empty() && !address.repoId.empty())) {
        logError(logPrefixParam + "Invalid link address format");
        return COMPONENT_ERROR;
    }
    
    // Create repo with the given repoId from address
    std::string initialCommitSha = address.initialCommitSha.empty() ? "main" : address.initialCommitSha;
    
    std::string fullRepoId = hfUsername_ + "/" + address.repoId;
    if (hfStub_->repo_exists(fullRepoId)) {
        // Reuse existing repo: do not create another initial commit.
        auto commits = hfStub_->list_commits(fullRepoId, "");
        if (!commits.empty()) {
            initialCommitSha = commits.front().sha;
        } else if (address.initialCommitSha.empty()) {
            initialCommitSha = "main";
        } else {
            initialCommitSha = address.initialCommitSha;
        }
        reusedRepo = true;
        logInfo(logPrefixParam + "Reusing existing repo " + fullRepoId + ", cursor=" + initialCommitSha);
    } else {
        try {
            // Create repo via HF stub using the address's repoId
            auto repoInfo = hfStub_->create_repo(address.repoId, false, false);
            if (!repoInfo.commitSha.empty()) {
                initialCommitSha = repoInfo.commitSha;
            }

            // Create initial commit with placeholder file (uses commit API, not large file upload)
            initialCommitSha = hfStub_->create_initial_commit(fullRepoId, "Initial commit");

        } catch (const HfError& e) {
            // Repo might already exist (race), fall back to latest commit
            logInfo(logPrefixParam + "Repo creation note (may already exist): " + std::string(e.what()));
            auto commits = hfStub_->list_commits(fullRepoId, "");
            if (!commits.empty()) {
                initialCommitSha = commits.front().sha;
            }
        } catch (const std::exception& e) {
            logError(logPrefixParam + "Repo creation failed (std::exception): " + std::string(e.what()));
            return COMPONENT_ERROR;
        } catch (...) {
            logError(logPrefixParam + "Repo creation failed (unknown exception)");
            return COMPONENT_ERROR;
        }
    }
    
    // Update address with actual commit SHA
    address.initialCommitSha = initialCommitSha;
    
    // Create link instance (isCreator = true for sender/creator)
    auto link = createLinkInstance(linkId, address, defaultLinkProperties_, true);
    if (reusedRepo) {
        link->setOwnsRepo(false);
    }
    
    // Reset cursor to initial commit (overwrite any stale persisted cursor from previous runs)
    // The listener/creator should always start fresh from the initial commit
    link->resetCursorToInitial(initialCommitSha);
    
    // Persist address for reload
    psh::saveValue(sdk_, psh::addressKey(linkId), linkAddress);
    
    return postLinkCreate(logPrefixParam, handle, linkId, link, LINK_CREATED);
}

ComponentStatus PluginHFTransport::destroyLink(RaceHandle handle, const LinkID& linkId) {
    std::string logPrefixParam = "destroyLink(" + linkId + "): ";
    TRACE_METHOD();
    
    auto link = links_.get(linkId);
    if (!link) {
        logWarning(logPrefixParam + "Link not found");
        return COMPONENT_ERROR;
    }
    
    bool isCreator = link->isCreator();
    
    // Sender deletes repo, receiver just cleans up locally
    if (isCreator && link->ownsRepo() && hfStub_) {
        try {
            hfStub_->delete_repo(hfUsername_ + "/" + link->getAddress().repoId);
            logInfo("Deleted repo: " + link->getAddress().repoId);
        } catch (const HfError& e) {
            logWarning("Failed to delete repo: " + std::string(e.what()));
        }
    }
    
    // Cleanup persistent storage
    psh::deleteValue(sdk_, psh::cursorKey(linkId));
    psh::deleteValue(sdk_, psh::headKey(linkId));
    psh::deleteValue(sdk_, psh::addressKey(linkId));
    
    link->shutdown();
    links_.remove(linkId);
    
    LinkParameters params;
    json paramsJson;
    paramsJson["linkId"] = linkId;
    params.json = paramsJson.dump();
    sdk_->onLinkStatusChanged(handle, linkId, LINK_DESTROYED, params);
    
    logInfo(logPrefixParam + "Link destroyed");
    return COMPONENT_OK;
}

std::vector<EncodingParameters> PluginHFTransport::getActionParams(const Action& action) {
    TRACE_METHOD();
    
    std::vector<EncodingParameters> result;
    try {
        ActionJson actionJson = json::parse(action.json);
        std::string actionType = actionJson.value("type", "");
        
        if (actionType == "post") {
            EncodingParameters params;
            params.linkId = actionJson.value("linkId", "");
            params.type = "application/octet-stream";
            params.encodePackage = true;
            params.json = "{}";
            result.push_back(params);
        } else if (actionType == "fetch") {
            EncodingParameters params;
            params.linkId = actionJson.value("linkId", "");
            params.type = "application/octet-stream";
            params.encodePackage = false;
            params.json = "{}";
            result.push_back(params);
        }
    } catch (const std::exception& e) {
        logError("Failed to parse action JSON: " + std::string(e.what()));
    }
    
    return result;
}

ComponentStatus PluginHFTransport::enqueueContent(const EncodingParameters& params, const Action& action, const std::vector<uint8_t>& content) {
    TRACE_METHOD();
    
    std::string linkId = params.linkId;
    // Prefer a specific link id supplied by the framework over a wildcard
    // from the action JSON, so content is queued on the correct link.
    std::string actionType = "";
    if (linkId.empty()) {
        // Parse from action JSON (framework may not populate params.linkId)
        try {
            nlohmann::json j = nlohmann::json::parse(action.json);
            if (j.contains("linkId")) {
                linkId = j.at("linkId").get<std::string>();
            }
            if (j.contains("type")) {
                actionType = j.at("type").get<std::string>();
            }
        } catch (const std::exception&) {
            // Ignore parse errors
        }
    }
    
    // For FETCH actions with wildcard linkId, there's no content to enqueue
    // (FETCH actions poll for incoming data, they don't send)
    if (actionType == "fetch" && linkId == "*") {
        return COMPONENT_OK;
    }
    
    // For POST actions with wildcard linkId, enqueue content on all links
    // TODO: Fix this - content should only be enqueued on the link that the content is for. Otherwise content encoded with an outgoing package may be sent to all clients rather than just the intended recipient.
    // Clarification: can we determine this information from the parameters of this function?
    if (actionType == "post" && linkId == "*") {
        ComponentStatus lastStatus = COMPONENT_OK;
        auto linkIds = links_.getAllIds();
        for (const auto& id : linkIds) {
            auto l = links_.get(id);
            if (l) {
                ComponentStatus status = l->enqueueContent(action.actionId, content);
                if (status != COMPONENT_OK) {
                    lastStatus = status;
                }
            }
        }
        return lastStatus;
    }
    
    if (linkId.empty()) {
        logError("Link not found: linkId is empty (not in params or action JSON)");
        return COMPONENT_ERROR;
    }
    
    auto link = links_.get(linkId);
    if (!link) {
        logError("Link not found: " + linkId);
        return COMPONENT_ERROR;
    }
    
    return link->enqueueContent(action.actionId, content);
}

ComponentStatus PluginHFTransport::dequeueContent(const Action& action) {
    TRACE_METHOD();
    
    try {
        ActionJson actionJson = json::parse(action.json);
        std::string linkId = actionJson.value("linkId", "");
        
        auto link = links_.get(linkId);
        if (!link) {
            return COMPONENT_ERROR;
        }
        
        return link->dequeueContent(action.actionId);
    } catch (const std::exception& e) {
        logError("Failed to parse action JSON: " + std::string(e.what()));
        return COMPONENT_ERROR;
    }
}

ComponentStatus PluginHFTransport::doAction(const std::vector<RaceHandle>& handles, const Action& action) {
    TRACE_METHOD();
    
    try {
        ActionJson actionJson = json::parse(action.json);
        std::string actionType = actionJson.value("type", "");
        std::string linkId = actionJson.value("linkId", "");
        
        // Handle wildcard linkId ("*") for FETCH actions - fetch on all links
        if (actionType == "fetch" && linkId == "*") {
            ComponentStatus lastStatus = COMPONENT_OK;
            auto linkIds = links_.getAllIds();
            for (const auto& id : linkIds) {
                auto link = links_.get(id);
                if (link) {
                    ComponentStatus status = link->fetch(handles);
                    if (status != COMPONENT_OK) {
                        lastStatus = status;
                    }
                }
            }
            return lastStatus;
        }
        
        // Handle wildcard linkId ("*") for POST actions - post on all links
        if (actionType == "post" && linkId == "*") {
            ComponentStatus lastStatus = COMPONENT_OK;
            auto linkIds = links_.getAllIds();
            for (const auto& id : linkIds) {
                auto l = links_.get(id);
                if (l) {
                    ComponentStatus status = l->post(handles, action.actionId);
                    if (status != COMPONENT_OK) {
                        lastStatus = status;
                    }
                }
            }
            return lastStatus;
        }
        
        auto link = links_.get(linkId);
        if (!link) {
            logError("Link not found: " + linkId);
            return COMPONENT_ERROR;
        }
        
        if (actionType == "post") {
            return link->post(handles, action.actionId);
        } else if (actionType == "fetch") {
            return link->fetch(handles);
        } else if (actionType == "cleanup") {
            // Cleanup is handled by destroyLink via User Model
            return COMPONENT_OK;
        }
        
        logError("Unknown action type: " + actionType);
        return COMPONENT_ERROR;
    } catch (const std::exception& e) {
        logError("Failed to parse action JSON: " + std::string(e.what()));
        return COMPONENT_ERROR;
    }
}

std::shared_ptr<Link> PluginHFTransport::createLinkInstance(const LinkID& linkId, const HfLinkAddress& address, const LinkProperties& properties, bool isCreator) {
    TRACE_METHOD();
    return std::make_shared<Link>(linkId, address, properties, isCreator, this, sdk_);
}

} // namespace hf_transport

#ifndef TESTBUILD
extern "C" {

ITransportComponent* createTransport(const std::string& transport, ITransportSdk* sdk,
                                     const std::string& roleName,
                                     const PluginConfig& pluginConfig) {
    (void)transport;
    (void)roleName;
    (void)pluginConfig;
    return new hf_transport::PluginHFTransport(sdk, roleName);
}

void destroyTransport(ITransportComponent* component) {
    delete component;
}

const RaceVersionInfo raceVersion = RACE_VERSION;

} // extern "C"
#endif