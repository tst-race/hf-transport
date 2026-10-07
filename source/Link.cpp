//
// HF Stego Transport - Link Implementation
//

#include "Link.h"
#include "RaceLog.h"
#include <ITransportSdk.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

Link::Link(LinkID linkId, LinkAddress address, LinkProperties properties,
           LinkDir direction, ITransportSdk* sdk,
           std::shared_ptr<HFTransportStub> stub)
    : sdk_(sdk), stub_(stub), linkId_(std::move(linkId)), address_(std::move(address)),
      properties_(std::move(properties)), direction_(direction),
      state_(std::make_shared<LinkState>()) {
    
    state_->linkId = linkId_;
    state_->address = address_;
    state_->direction = direction_;
    state_->createdAt = std::chrono::steady_clock::now();
    state_->lastActivity = state_->createdAt;
    
    RaceLog::logInfo("Link", "Created link " + linkId_ + " (" + 
            (direction_ == LinkDir::SEND ? "SEND" : 
             direction_ == LinkDir::RECEIVE ? "RECEIVE" : "BOTH") + 
            ") for repo " + address_.getRepoId(), "");
}

Link::~Link() {
    shutdown();
}

ComponentStatus Link::enqueueContent(uint64_t actionId, const std::vector<uint8_t>& content) {
    std::lock_guard<std::mutex> lock(state_->contentMutex);
    state_->contentQueue[actionId] = content;
    RaceLog::logDebug("Link", "Enqueued content for action " + std::to_string(actionId) + 
             " (" + std::to_string(content.size()) + " bytes)", "");
    return COMPONENT_OK;
}

ComponentStatus Link::dequeueContent(uint64_t actionId) {
    std::lock_guard<std::mutex> lock(state_->contentMutex);
    state_->contentQueue.erase(actionId);
    RaceLog::logDebug("Link", "Dequeued content for action " + std::to_string(actionId), "");
    return COMPONENT_OK;
}

std::optional<std::vector<uint8_t>> Link::getContent(uint64_t actionId) {
    std::lock_guard<std::mutex> lock(state_->contentMutex);
    auto it = state_->contentQueue.find(actionId);
    if (it != state_->contentQueue.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Link::setPendingPayload(const std::vector<uint8_t>& payload) {
    std::lock_guard<std::mutex> lock(state_->contentMutex);
    state_->pendingPayload = payload;
    state_->hasPendingPayload = true;
    RaceLog::logDebug("Link", "Set pending payload (" + std::to_string(payload.size()) + " bytes)", "");
}

std::vector<uint8_t> Link::takePendingPayload() {
    std::lock_guard<std::mutex> lock(state_->contentMutex);
    std::vector<uint8_t> payload = std::move(state_->pendingPayload);
    state_->hasPendingPayload = false;
    return payload;
}

void Link::start() {
    if (isShutdown_) return;
    isShutdown_ = false;
    thread_ = std::thread(&Link::runActionThread, this);
    RaceLog::logInfo("Link", "Started action thread for link " + linkId_, "");
}

void Link::shutdown() {
    if (isShutdown_) return;
    isShutdown_ = true;
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    state_->active = false;
    RaceLog::logInfo("Link", "Shutdown link " + linkId_, "");
}

void Link::touchActivity() {
    state_->lastActivity = std::chrono::steady_clock::now();
}

void Link::runActionThread() {
    RaceLog::logInfo("Link", "Action thread started for link " + linkId_, "");
    
    while (!isShutdown_) {
        std::function<void()> action;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, 100ms, [this] { return isShutdown_ || !actionQueue_.empty(); });
            
            if (isShutdown_) break;
            if (actionQueue_.empty()) continue;
            
            action = std::move(actionQueue_.front());
            actionQueue_.pop_front();
        }
        
        if (action) {
            try {
                action();
            } catch (const std::exception& e) {
                RaceLog::logError("Link", "Action thread exception: " + std::string(e.what()), "");
            }
        }
    }
    
    RaceLog::logInfo("Link", "Action thread stopped for link " + linkId_, "");
}

void Link::updatePackageStatus(const std::vector<RaceHandle>& handles, PackageStatus status) {
    for (const auto& handle : handles) {
        sdk_->onPackageStatusChanged(handle, status);
    }
}

ComponentStatus Link::post(std::vector<RaceHandle> handles, uint64_t actionId) {
    RaceLog::logInfo("Link", "POST action " + std::to_string(actionId) + " for link " + linkId_, "");
    
    // Get the stego model data from content queue
    auto content = getContent(actionId);
    if (!content.has_value()) {
        RaceLog::logError("Link", "No content queued for action " + std::to_string(actionId), "");
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        return COMPONENT_ERROR;
    }
    
    // Upload to HF Hub
    ComponentStatus status = uploadModel(content.value());
    if (status == COMPONENT_OK) {
        updatePackageStatus(handles, PACKAGE_SENT);
        RaceLog::logInfo("Link", "POST successful for action " + std::to_string(actionId), "");
    } else {
        updatePackageStatus(handles, PACKAGE_FAILED_GENERIC);
        RaceLog::logError("Link", "POST failed for action " + std::to_string(actionId), "");
    }
    
    return status;
}

ComponentStatus Link::fetch() {
    RaceLog::logDebug("Link", "FETCH for link " + linkId_, "");
    
    // Download from HF Hub
    std::vector<uint8_t> modelData;
    ComponentStatus status = downloadModel(modelData);
    
    if (status == COMPONENT_OK && !modelData.empty()) {
        // Pass downloaded data to encoding module via onReceive
        // Use encoding parameters matching the sender's POST encoding (carrier_model)
        nlohmann::json paramsJson;
        paramsJson["carrier_model"] = address_.carrier_model;
        paramsJson["action_type"] = "fetch";
        paramsJson["linkId"] = linkId_;
        
        EncodingParameters encodingParams;
        encodingParams.linkId = linkId_;
        encodingParams.type = "text/plain";  // Base64 encoding outputs text/plain
        encodingParams.encodePackage = false;
        encodingParams.json = paramsJson.dump();
        
        RaceLog::logInfo("Link", "FETCH successful for link " + linkId_ + 
                " (" + std::to_string(modelData.size()) + " bytes), calling onReceive", "");
        
        sdk_->onReceive(linkId_, encodingParams, modelData);
    } else {
        RaceLog::logDebug("Link", "FETCH no new content for link " + linkId_, "");
    }
    
    return status;
}

ComponentStatus Link::cleanup() {
    RaceLog::logInfo("Link", "CLEANUP for link " + linkId_, "");
    
    if (direction_ == LinkDir::SEND || direction_ == LinkDir::BOTH) {
        return deleteRepo();
    }
    
    return COMPONENT_OK;
}

ComponentStatus Link::doPost(uint64_t actionId) {
    // This is called from action thread context
    std::vector<RaceHandle> handles; // Would be passed from doAction
    return post(std::move(handles), actionId);
}

ComponentStatus Link::doFetch() {
    return fetch();
}

ComponentStatus Link::doCleanup() {
    return cleanup();
}

ComponentStatus Link::uploadModel(const std::vector<uint8_t>& modelData) {
    if (!stub_) {
        RaceLog::logError("Link", "No HF stub available", "");
        return COMPONENT_ERROR;
    }
    
    try {
        // Write model data to temporary directory
        std::filesystem::path tempDir = std::filesystem::temp_directory_path() / 
                                        ("hf_stego_upload_" + linkId_);
        std::filesystem::create_directories(tempDir);
        
        // The modelData is already a complete carrier directory structure
        // (produced by encoding module). We need to extract it.
        // For now, assume it's a tar/zip or we write individual files.
        // Simplified: write as single file for now.
        std::filesystem::path modelFile = tempDir / "model.safetensors";
        std::ofstream ofs(modelFile, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(modelData.data()), modelData.size());
        ofs.close();
        
        // Upload folder
        auto result = stub_->upload_folder(tempDir, address_.getRepoId(), 
                                           "racegen: post stego model");
        
        state_->lastCommitSha = result.commit_sha;
        touchActivity();
        
        // Cleanup temp dir
        std::filesystem::remove_all(tempDir);
        
        RaceLog::logInfo("Link", "Uploaded model to " + address_.getRepoId() + 
                " commit " + result.commit_sha.substr(0, 8), "");
        
        return COMPONENT_OK;
    } catch (const std::exception& e) {
        RaceLog::logError("Link", "Upload failed: " + std::string(e.what()), "");
        return COMPONENT_ERROR;
    }
}

ComponentStatus Link::downloadModel(std::vector<uint8_t>& modelData) {
    if (!stub_) {
        RaceLog::logError("Link", "No HF stub available", "");
        return COMPONENT_ERROR;
    }
    
    try {
        std::filesystem::path tempDir = std::filesystem::temp_directory_path() / 
                                        ("hf_stego_download_" + linkId_);
        std::filesystem::create_directories(tempDir);
        
        // Download repo
        auto result = stub_->download_repo(address_.getRepoId(), tempDir);
        
        state_->lastCommitSha = result.commit_sha;
        touchActivity();
        
        // Read the model file (simplified - in reality would read all carrier files)
        std::filesystem::path modelFile = fs::path(result.local_path) / "model.safetensors";
        if (std::filesystem::exists(modelFile)) {
            std::ifstream ifs(modelFile, std::ios::binary | std::ios::ate);
            std::streamsize size = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            
            modelData.resize(size);
            ifs.read(reinterpret_cast<char*>(modelData.data()), size);
        }
        
        // Cleanup temp dir
        std::filesystem::remove_all(tempDir);
        
        RaceLog::logDebug("Link", "Downloaded model from " + address_.getRepoId() + 
                 " (" + std::to_string(modelData.size()) + " bytes)", "");
        
        return COMPONENT_OK;
    } catch (const HFTransportStub::NotFoundError& e) {
        RaceLog::logWarning("Link", "Repo not found (may have been deleted): " + address_.getRepoId(), "");
        return COMPONENT_ERROR;
    } catch (const std::exception& e) {
        RaceLog::logError("Link", "Download failed: " + std::string(e.what()), "");
        return COMPONENT_ERROR;
    }
}

ComponentStatus Link::deleteRepo() {
    if (!stub_) {
        RaceLog::logError("Link", "No HF stub available", "");
        return COMPONENT_ERROR;
    }
    
    try {
        bool deleted = stub_->delete_repo(address_.getRepoId());
        if (deleted) {
            RaceLog::logInfo("Link", "Deleted repo " + address_.getRepoId(), "");
        }
        return deleted ? COMPONENT_OK : COMPONENT_ERROR;
    } catch (const std::exception& e) {
        RaceLog::logError("Link", "Delete failed: " + std::string(e.what()), "");
        return COMPONENT_ERROR;
    }
}