//
// HF Stego Transport - Link Address
//

#ifndef HF_STEGO_TRANSPORT_LINK_ADDRESS_H
#define HF_STEGO_TRANSPORT_LINK_ADDRESS_H

#include <nlohmann/json.hpp>
#include <string>
#include <optional>

struct LinkAddress {
    // Required
    std::string username;      // HF username
    std::string repo_name;     // transport-{uuid}
    
    // Optional
    std::string commit_sha;    // Git commit SHA (empty = main branch)
    std::string carrier_model; // Base model used (for verification)
    
    // Constructor
    LinkAddress() = default;
    LinkAddress(const std::string& u, const std::string& r, const std::string& c = "", const std::string& m = "")
        : username(u), repo_name(r), commit_sha(c), carrier_model(m) {}
    
    // Convert to hf:// address string
    std::string toString() const {
        std::string addr = "hf://" + username + "/" + repo_name;
        if (!commit_sha.empty()) {
            addr += "@" + commit_sha;
        }
        return addr;
    }
    
    // Parse from hf:// address string
    static std::optional<LinkAddress> fromString(const std::string& address) {
        if (address.rfind("hf://", 0) != 0) {
            return std::nullopt;
        }
        
        std::string path = address.substr(5); // Remove "hf://"
        std::string commit_sha = "";
        
        size_t at_pos = path.find('@');
        if (at_pos != std::string::npos) {
            commit_sha = path.substr(at_pos + 1);
            path = path.substr(0, at_pos);
        }
        
        size_t slash_pos = path.find('/');
        if (slash_pos == std::string::npos) {
            return std::nullopt;
        }
        
        std::string username = path.substr(0, slash_pos);
        std::string repo_name = path.substr(slash_pos + 1);
        
        if (username.empty() || repo_name.empty()) {
            return std::nullopt;
        }
        
        return LinkAddress(username, repo_name, commit_sha);
    }
    
    // Get full repo ID (username/repo_name)
    std::string getRepoId() const {
        return username + "/" + repo_name;
    }
    
    // JSON serialization
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["username"] = username;
        j["repo_name"] = repo_name;
        if (!commit_sha.empty()) j["commit_sha"] = commit_sha;
        if (!carrier_model.empty()) j["carrier_model"] = carrier_model;
        return j;
    }
    
    static LinkAddress fromJson(const nlohmann::json& j) {
        LinkAddress addr;
        addr.username = j.at("username").get<std::string>();
        addr.repo_name = j.at("repo_name").get<std::string>();
        addr.commit_sha = j.value("commit_sha", "");
        addr.carrier_model = j.value("carrier_model", "");
        return addr;
    }
};

inline void to_json(nlohmann::json& destJson, const LinkAddress& src) {
    destJson = src.toJson();
}

inline void from_json(const nlohmann::json& srcJson, LinkAddress& dest) {
    dest = LinkAddress::fromJson(srcJson);
}

#endif // HF_STEGO_TRANSPORT_LINK_ADDRESS_H