#ifndef HF_LINK_ADDRESS_H
#define HF_LINK_ADDRESS_H

#include <string>
#include <nlohmann/json.hpp>

namespace hf_transport {

/**
 * Link address for HF Hub transport.
 * 
 * Format: hf://{username}/transport-{uuid}@{initialCommitSha}?branch={branch}
 * 
 * The initialCommitSha is the BASELINE commit at link creation time.
 * Runtime cursor (lastFetchedCommitSha) is tracked separately in persistent storage.
 */
struct HfLinkAddress {
    // REQUIRED: Service-specific fields (exchanged in link address)
    std::string username;           // HF username (repo owner) — from user input
    std::string repoId;             // e.g., "transport-a1b2c3d4-e5f6" (UUID for isolation)
    std::string initialCommitSha;   // Git commit SHA at link creation (baseline for cursor)
    std::string branch = "main";    // Git branch (default "main")
    
    // OPTIONAL: Transport configuration
    int maxTries = 120;             // Max fetch attempts before giving up
    int openObjects = 1;            // Number of concurrent objects (unused for HF)
    bool singleReceive = false;     // Delete after receive? (not applicable for HF)
    
    // Default constructor
    HfLinkAddress() = default;
    
    // Constructor with required fields
    HfLinkAddress(const std::string& username_,
                  const std::string& repoId_,
                  const std::string& initialCommitSha_,
                  const std::string& branch_ = "main")
        : username(username_), repoId(repoId_), initialCommitSha(initialCommitSha_), branch(branch_) {}
    
    // Check if address has minimum required fields
    bool isValid() const {
        return !username.empty() && !repoId.empty() && !initialCommitSha.empty();
    }
    
    // Serialize to JSON string format (for framework compatibility)
    std::string toJsonString() const {
        nlohmann::json j;
        j["type"] = "hf";
        j["username"] = username;
        j["repoId"] = repoId;
        j["commitSha"] = initialCommitSha;
        j["branch"] = branch;
        return j.dump();
    }
    
    // Serialize to hf:// string format (legacy)
    std::string toString() const {
        return toJsonString();
    }
    
    // Parse from hf:// string format
    // Expected: hf://username/repoId@commitSha[?branch=branch]
    static HfLinkAddress fromString(const std::string& addr) {
        HfLinkAddress result;
        
        if (addr.rfind("hf://", 0) != 0) {
            return result;  // Invalid format, return empty
        }
        
        std::string path = addr.substr(5);  // Remove "hf://"
        
        // Split branch query parameter
        std::string branch = "main";
        size_t queryPos = path.find('?');
        if (queryPos != std::string::npos) {
            std::string query = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);
            // Parse query parameters (simple branch=X for now)
            if (query.rfind("branch=", 0) == 0) {
                branch = query.substr(7);
            }
        }
        
        // Split commit SHA
        std::string commitSha = "main";
        size_t atPos = path.rfind('@');
        if (atPos != std::string::npos) {
            commitSha = path.substr(atPos + 1);
            path = path.substr(0, atPos);
        }
        
        // Split username/repoId
        size_t slashPos = path.find('/');
        if (slashPos != std::string::npos) {
            result.username = path.substr(0, slashPos);
            result.repoId = path.substr(slashPos + 1);
        }
        
        result.initialCommitSha = commitSha;
        result.branch = branch;
        
        return result;
    }
    
    // Parse from JSON format (used by framework for connection reuse)
    // Expected: {"type": "hf", "username": "...", "repoId": "...", "commitSha": "...", "branch": "..."}
    static HfLinkAddress fromJson(const std::string& jsonStr) {
        HfLinkAddress result;
        try {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            
            // Handle both formats:
            // 1. Full format with type field: {"type": "hf", "username": "...", "repoId": "...", "commitSha": "...", "branch": "..."}
            // 2. Direct format: {"username": "...", "repoId": "...", "initialCommitSha": "...", "branch": "..."}
            
            if (j.contains("type") && j.at("type") == "hf") {
                // Full format with type field
                j.at("username").get_to(result.username);
                j.at("repoId").get_to(result.repoId);
                if (j.contains("commitSha")) {
                    j.at("commitSha").get_to(result.initialCommitSha);
                } else if (j.contains("initialCommitSha")) {
                    j.at("initialCommitSha").get_to(result.initialCommitSha);
                }
                if (j.contains("branch")) {
                    j.at("branch").get_to(result.branch);
                }
            } else {
                // Direct format (from JSON serialization)
                j.at("username").get_to(result.username);
                j.at("repoId").get_to(result.repoId);
                j.at("initialCommitSha").get_to(result.initialCommitSha);
                if (j.contains("branch")) {
                    j.at("branch").get_to(result.branch);
                }
            }
            
            // Optional fields with defaults
            if (j.contains("maxTries")) {
                j.at("maxTries").get_to(result.maxTries);
            }
            if (j.contains("openObjects")) {
                j.at("openObjects").get_to(result.openObjects);
            }
            if (j.contains("singleReceive")) {
                j.at("singleReceive").get_to(result.singleReceive);
            }
        } catch (const std::exception& e) {
            // Invalid JSON, return empty
        }
        return result;
    }
    
    // Unified parse - tries JSON first, then hf:// string format
    static HfLinkAddress parse(const std::string& addr) {
        // Try JSON first (framework format)
        if (!addr.empty() && addr[0] == '{') {
            return fromJson(addr);
        }
        // Try hf:// format
        return fromString(addr);
    }
    
    // JSON serialization (nlohmann::json)
    friend void to_json(nlohmann::json& j, const HfLinkAddress& addr) {
        j = nlohmann::json{
            {"username", addr.username},
            {"repoId", addr.repoId},
            {"initialCommitSha", addr.initialCommitSha},
            {"branch", addr.branch},
            {"maxTries", addr.maxTries},
            {"openObjects", addr.openObjects},
            {"singleReceive", addr.singleReceive}
        };
    }
    
    friend void from_json(const nlohmann::json& j, HfLinkAddress& addr) {
        j.at("username").get_to(addr.username);
        j.at("repoId").get_to(addr.repoId);
        j.at("initialCommitSha").get_to(addr.initialCommitSha);
        
        // Optional fields with defaults
        if (j.contains("branch")) {
            j.at("branch").get_to(addr.branch);
        }
        if (j.contains("maxTries")) {
            j.at("maxTries").get_to(addr.maxTries);
        }
        if (j.contains("openObjects")) {
            j.at("openObjects").get_to(addr.openObjects);
        }
        if (j.contains("singleReceive")) {
            j.at("singleReceive").get_to(addr.singleReceive);
        }
    }
};

} // namespace hf_transport

#endif // HF_LINK_ADDRESS_H