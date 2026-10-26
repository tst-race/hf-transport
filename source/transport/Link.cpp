#include "Link.h"
#include "PluginHFTransport.h"
#include "log.h"
#include "PersistentStorageHelpers.h"
#include <ITransportSdk.h>
#include <sstream>
#include <fstream>

namespace hf_transport {

Link::Link(const LinkID& linkId, 
           const HfLinkAddress& address, 
           const LinkProperties& properties, 
           bool isCreator, 
           PluginHFTransport* transport, 
           ITransportSdk* sdk)
    : sdk_(sdk), transport_(transport), linkId_(linkId), properties_(properties), address_(address), isCreator_(isCreator) {
    TRACE_METHOD();
    
    // Initialize runtime state
    currentHeadCommitSha_ = address_.initialCommitSha;
    
    // Load persisted cursor
    loadCursor();
    
    logInfo("Link created: " + linkId + " (creator=" + (isCreator ? "true" : "false") + ", repo=" + address_.repoId + ")");
}

Link::~Link() {
    TRACE_METHOD();
    shutdown();
}

ComponentStatus Link::enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content) {
    TRACE_METHOD();
    logDebug("enqueueContent: actionId=" + std::to_string(actionId) + " contentSize=" + std::to_string(content.size()));
    
    std::lock_guard<std::mutex> lock(mutex_);
    contentQueue_[actionId] = content;
    return COMPONENT_OK;
}

ComponentStatus Link::dequeueContent(uint64_t actionId) {
    TRACE_METHOD();
    
    std::lock_guard<std::mutex> lock(mutex_);
    contentQueue_.erase(actionId);
    return COMPONENT_OK;
}

ComponentStatus Link::fetch(std::vector<RaceHandle> handles) {
    TRACE_METHOD();
    
    std::lock_guard<std::mutex> lock(mutex_);
    actionQueue_.push_back({false, std::move(handles), 0});  // actionId not used for fetch
    cv_.notify_one();
    return COMPONENT_OK;
}

ComponentStatus Link::post(std::vector<RaceHandle> handles, uint64_t actionId) {
    TRACE_METHOD();
    
    std::lock_guard<std::mutex> lock(mutex_);
    actionQueue_.push_back({true, std::move(handles), actionId});
    cv_.notify_one();
    return COMPONENT_OK;
}

void Link::start() {
    TRACE_METHOD();
    if (thread_.joinable()) return;
    isShutdown_ = false;
    thread_ = std::thread(&Link::runActionThread, this);
    logInfo("Link action thread started: " + linkId_);
}

void Link::shutdown() {
    TRACE_METHOD();
    isShutdown_ = true;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    logInfo("Link action thread stopped: " + linkId_);
}

void Link::runActionThread() {
    TRACE_METHOD();
    
    while (!isShutdown_) {
        QueuedAction action;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return isShutdown_ || !actionQueue_.empty(); });
            
            if (isShutdown_) break;
            
            action = std::move(actionQueue_.front());
            actionQueue_.pop_front();
        }
        
        try {
            if (action.post) {
                currentHeadCommitSha_ = postOnActionThread(currentHeadCommitSha_, action.handles, action.actionId);
            } else {
                lastFetchedCommitSha_ = fetchOnActionThread(lastFetchedCommitSha_);
                saveCursor();  // Persist after successful FETCH
            }
        } catch (const HfError& e) {
            logError("Action failed for link " + linkId_ + ": " + e.what());
            PackageStatus status = mapHfError(e);
            updatePackageStatus(action.handles, status);
            emitEvent("failed", action.actionId);
        } catch (const std::exception& e) {
            logError("Unexpected error in link " + linkId_ + ": " + e.what());
            updatePackageStatus(action.handles, PACKAGE_FAILED_GENERIC);
            emitEvent("failed", action.actionId);
        }
    }
}

std::string Link::postOnActionThread(const std::string& currentHeadSha, const std::vector<RaceHandle>& handles, uint64_t actionId) {
    TRACE_METHOD();
    logDebug("postOnActionThread: actionId=" + std::to_string(actionId) + " contentQueueSize=" + std::to_string(contentQueue_.size()));
    
    // Get content from queue
    std::vector<uint8_t> content;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = contentQueue_.find(actionId);
        if (it == contentQueue_.end()) {
            // No content queued for this link/action (e.g. wildcard post on a
            // link that did not receive an EnqueueData). Skip without failing.
            logDebug("No content queued for actionId " + std::to_string(actionId) + " on link " + linkId_ + "; skipping post");
            return currentHeadSha;
        }
        content = std::move(it->second);
        contentQueue_.erase(it);
    }
    
    if (content.empty()) {
        logWarning("Empty content for actionId " + std::to_string(actionId));
        updatePackageStatus(handles, PACKAGE_SENT);
        emitEvent("posted", actionId);
        return currentHeadSha;
    }
    
    std::string newCommitSha;
    if (!uploadContent(content, "racegen message", newCommitSha)) {
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        emitEvent("failed", actionId);
        return currentHeadSha;
    }
    
    updatePackageStatus(handles, PACKAGE_SENT);
    emitEvent("posted", actionId);
    
    logInfo("Posted to " + address_.repoId + " new commit: " + newCommitSha);
    return newCommitSha;
}

bool Link::uploadContent(const std::vector<uint8_t>& content, const std::string& commitMessage, std::string& newCommitSha) {
    TRACE_METHOD();
    
    try {
        // Create temporary directory with content
        std::string tempDir = "/tmp/hf_transport_" + linkId_ + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(tempDir);
        
        // Write content as a single file (transport is carrier-agnostic)
        std::string contentFile = tempDir + "/payload.bin";
        std::ofstream ofs(contentFile, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(content.data()), static_cast<std::streamsize>(content.size()));
        ofs.close();
        
        // Upload via HfTransportStub using git-based commit (more reliable than large file upload)
        HfTransportStub* stub = transport_->getHfStub();
        if (!stub) {
            logError("HF stub not initialized");
            return false;
        }
        
        // Use full repo ID (username/repoId)
        std::string fullRepoId = address_.username + "/" + address_.repoId;
        newCommitSha = stub->commit_content(fullRepoId, tempDir, commitMessage);
        
        // Cleanup temp directory
        std::filesystem::remove_all(tempDir);
        
        return true;
    } catch (const HfError& e) {
        logError("Upload failed: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        logError("Upload failed: " + std::string(e.what()));
        return false;
    }
}

std::string Link::fetchOnActionThread(const std::string& lastFetchedSha) {
    TRACE_METHOD();
    // Creator (send) links are not expected to receive messages; skip fetching
    // to avoid delivering our own posted commits as replies (which would
    // prematurely finish a send-receive state machine).
    // TODO: Implement self-send detection to support executing network actions corresponding to fetch without actually forwarding our own messages to ourselves.
    if (isCreator_) {
        return lastFetchedSha;
    }
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> newMessages;
    if (!downloadNewCommits(lastFetchedSha, newMessages)) {
        return lastFetchedSha;
    }
    
    std::string newLastFetched = lastFetchedSha;
    for (const auto& [commitSha, content] : newMessages) {
        if (content.empty()) continue;
        
        // Create encoding parameters for the receive
        EncodingParameters params;
        params.linkId = linkId_;
        params.type = "application/octet-stream";
        params.encodePackage = false;
        params.json = "{}";
        
        // Log received message content for test verification
        std::string messageStr(content.begin(), content.end());
        logInfo("Received message on " + linkId_ + ": " + messageStr);
        
        // Deliver to Race SDK
        logDebug("Calling onReceive for link " + linkId_ + " contentSize=" + std::to_string(content.size()));
        sdk_->onReceive(linkId_, params, content);
        logDebug("onReceive returned for link " + linkId_);
        
        newLastFetched = commitSha;
        emitEvent("fetched", 0);
    }
    
    if (!newMessages.empty()) {
        logInfo("Fetched " + std::to_string(newMessages.size()) + " new messages from " + address_.repoId + ", cursor now at " + newLastFetched);
    }
    
    return newLastFetched;
}

bool Link::downloadNewCommits(const std::string& lastFetchedSha, std::vector<std::pair<std::string, std::vector<uint8_t>>>& newMessages) {
    TRACE_METHOD();
    
    try {
        HfTransportStub* stub = transport_->getHfStub();
        if (!stub) {
            logError("HF stub not initialized");
            return false;
        }
        
        // Get commits since last fetched - use full repo ID (username/repoId)
        std::string fullRepoId = address_.username + "/" + address_.repoId;
        
        // Check if repo exists first (for CREATOR_TO_LOADER, repo may not exist yet)
        if (!stub->repo_exists(fullRepoId)) {
            logDebug("Repository " + fullRepoId + " does not exist yet, will retry on next fetch");
            return true;  // No error, just no new messages - retry on next fetch
        }
        
        auto commits = stub->list_commits(fullRepoId, lastFetchedSha);
        if (commits.empty()) {
            return true;  // No new commits
        }
        
        // For each new commit, download and extract content
        for (const auto& commit : commits) {
            if (commit.sha == lastFetchedSha) continue;  // Skip baseline
            
            // Download this commit - use full repo ID
            std::string tempDir = "/tmp/hf_fetch_" + linkId_ + "_" + commit.sha;
            std::filesystem::create_directories(tempDir);
            
            try {
                auto result = stub->download_repo(fullRepoId, tempDir, commit.sha);
                
                // Extract payload.bin
                std::string contentFile = tempDir + "/payload.bin";
                if (std::filesystem::exists(contentFile)) {
                    std::ifstream ifs(contentFile, std::ios::binary);
                    std::vector<uint8_t> content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    newMessages.emplace_back(commit.sha, std::move(content));
                }
                
                // Cleanup
                std::filesystem::remove_all(tempDir);
            } catch (const std::exception& e) {
                logError("Failed to download commit " + commit.sha + ": " + e.what());
                std::filesystem::remove_all(tempDir);
                // Continue with next commit
            }
        }
        
        return true;
    } catch (const HfError& e) {
        // Check if it's a "not found" error (repo doesn't exist yet)
        std::string what = e.what();
        if (what.find("404") != std::string::npos || what.find("not found") != std::string::npos) {
            logDebug("Repository " + address_.username + "/" + address_.repoId + " not found (404), will retry on next fetch: " + what);
            return true;  // No error, just retry on next fetch
        }
        logError("Download new commits failed: " + what);
        throw;
    } catch (const std::exception& e) {
        logError("Download new commits failed: " + std::string(e.what()));
        return false;
    }
}

void Link::loadCursor() {
    std::string cursor = psh::readValue(sdk_, psh::cursorKey(linkId_), "");
    if (!cursor.empty()) {
        lastFetchedCommitSha_ = cursor;
        logInfo("Loaded cursor for " + linkId_ + ": " + cursor);
    } else {
        lastFetchedCommitSha_ = address_.initialCommitSha;
        logInfo("No cursor for " + linkId_ + ", using initial: " + lastFetchedCommitSha_);
    }
}

void Link::saveCursor() {
    if (!lastFetchedCommitSha_.empty()) {
        psh::saveValue(sdk_, psh::cursorKey(linkId_), lastFetchedCommitSha_);
        logDebug("Saved cursor for " + linkId_ + ": " + lastFetchedCommitSha_);
    }
}

void Link::resetCursorToInitial(const std::string& initialCommitSha) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string cursorSha = initialCommitSha;
    // If initialCommitSha is "main" or empty, resolve to actual first commit in repo
    if (cursorSha.empty() || cursorSha == "main") {
        try {
            HfTransportStub* stub = transport_->getHfStub();
            if (stub) {
                std::string fullRepoId = address_.username + "/" + address_.repoId;
                auto commits = stub->list_commits(fullRepoId, "");
                logDebug("resetCursorToInitial: list_commits returned " + std::to_string(commits.size()) + " commits");
                for (size_t i = 0; i < commits.size(); ++i) {
                    logDebug("  commit[" + std::to_string(i) + "] = " + commits[i].sha);
                }
                if (!commits.empty()) {
                    // Use the oldest commit (last in list since list_commits returns newest first)
                    cursorSha = commits.back().sha;
                    logInfo("Resolved initial commit for " + linkId_ + " to oldest: " + cursorSha);
                }
            }
        } catch (const std::exception& e) {
            logWarning("Failed to resolve initial commit, using provided: " + std::string(e.what()));
        }
    }
    
    lastFetchedCommitSha_ = cursorSha;
    psh::saveValue(sdk_, psh::cursorKey(linkId_), cursorSha);
    logInfo("Reset cursor for " + linkId_ + " to initial commit: " + cursorSha);
}

PackageStatus Link::mapHfError(const HfError& e) {
    std::string what = e.what();
    if (dynamic_cast<const AuthError*>(&e)) {
        return PACKAGE_FAILED_GENERIC;  // Will trigger user input for new token
    } else if (dynamic_cast<const NotFoundError*>(&e)) {
        return PACKAGE_FAILED_GENERIC;  // Link expired
    } else if (dynamic_cast<const RateLimitError*>(&e)) {
        return PACKAGE_FAILED_GENERIC;  // Schedule retry
    }
    return PACKAGE_FAILED_GENERIC;
}

void Link::updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status) {
    for (auto handle : handles) {
        sdk_->onPackageStatusChanged(handle, status);
    }
}

void Link::emitEvent(const std::string& status, uint64_t actionId) {
    nlohmann::json eventJson = {
        {"linkId", linkId_},
        {"actionId", actionId},
        {"status", status}
    };
    sdk_->onEvent({eventJson.dump()});
}

} // namespace hf_transport