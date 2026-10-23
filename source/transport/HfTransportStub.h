#ifndef HF_TRANSPORT_STUB_H
#define HF_TRANSPORT_STUB_H

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace hf_transport {

// Forward declaration
class HfTransportStub;

// Exception hierarchy
class HfError : public std::runtime_error {
public:
    explicit HfError(const std::string& msg) : std::runtime_error(msg) {}
};

class AuthError : public HfError { public: explicit AuthError(const std::string& msg) : HfError(msg) {} };
class NotFoundError : public HfError { public: explicit NotFoundError(const std::string& msg) : HfError(msg) {} };
class RateLimitError : public HfError { 
public: 
    explicit RateLimitError(const std::string& msg, int retryAfter = 0) : HfError(msg), retryAfter_(retryAfter) {}
    int retryAfter() const { return retryAfter_; }
private: int retryAfter_ = 0;
};
class ConflictError : public HfError { public: explicit ConflictError(const std::string& msg) : HfError(msg) {} };
class ServerError : public HfError { public: explicit ServerError(const std::string& msg) : HfError(msg) {} };

// Repository metadata
struct RepoInfo {
    std::string repoId;
    std::string author;
    std::string name;
    bool privateRepo = false;
    std::string commitSha;
    std::string url;
};

// Upload result
struct UploadResult {
    std::string repoId;
    std::string commitSha;
    std::vector<std::string> filesUploaded;
    std::string commitUrl;
};

// Download result
struct DownloadResult {
    std::string localPath;
    std::string repoId;
    std::string commitSha;
    std::vector<std::string> filesDownloaded;
};

// Commit info for cursor-based fetch
struct CommitInfo {
    std::string sha;
    std::string message;
    std::string timestamp;  // ISO 8601
};

class HfTransportStub {
public:
    HfTransportStub(const std::string& token, const std::string& username);
    ~HfTransportStub();

    // Repository lifecycle
    RepoInfo create_repo(const std::string& repoName, bool privateRepo = false, bool existOk = false);
    bool delete_repo(const std::string& repoId);
    bool repo_exists(const std::string& repoId);
    RepoInfo get_repo_info(const std::string& repoId);

    // File operations
    UploadResult upload_folder(const std::string& localPath, const std::string& repoId, const std::string& commitMessage);
    DownloadResult download_repo(const std::string& repoId, const std::string& localDir, const std::string& revision = "main");
    
    // Create initial commit with a placeholder file (uses commit API, not large file upload)
    std::string create_initial_commit(const std::string& repoId, const std::string& commitMessage = "Initial commit");
    
    // Commit content files via git (more reliable than large file upload API)
    std::string commit_content(const std::string& repoId, const std::string& localPath, const std::string& commitMessage);

    // Discovery & addressing
    std::vector<RepoInfo> list_user_repos(const std::string& prefix = "transport-");
    RepoInfo resolve_address(const std::string& hfAddress);  // "hf://user/repo@rev"
    std::string construct_address(const std::string& repoId, const std::string& commitSha);

    // Commit history (for cursor-based fetch)
    std::vector<CommitInfo> list_commits(const std::string& repoId, const std::string& sinceSha = "");

    // Rate limiting
    void set_rate_limit(int calls, int windowSeconds);

private:
    std::string token_;
    std::string username_;
    std::string apiBase_ = "https://huggingface.co/api";
    int rateLimitCalls_ = 1000;
    int rateLimitWindow_ = 300;  // 5 minutes
    
    // Simple token bucket rate limiter
    struct RateLimiter {
        int maxCalls;
        int windowSeconds;
        std::vector<std::chrono::steady_clock::time_point> calls;
        std::mutex mutex;
        
        RateLimiter(int maxCalls_, int windowSeconds_) : maxCalls(maxCalls_), windowSeconds(windowSeconds_) {}
        
        void acquire(int tokens = 1) {
            std::unique_lock<std::mutex> lock(mutex);
            auto now = std::chrono::steady_clock::now();
            auto cutoff = now - std::chrono::seconds(windowSeconds);
            
            // Remove old calls
            calls.erase(
                std::remove_if(calls.begin(), calls.end(), 
                    [&cutoff](const auto& t) { return t < cutoff; }),
                calls.end()
            );
            
            if (static_cast<int>(calls.size()) + tokens > maxCalls) {
                // Wait until oldest call expires
                if (!calls.empty()) {
                    auto waitUntil = calls.front() + std::chrono::seconds(windowSeconds);
                    auto waitDuration = waitUntil - now;
                    if (waitDuration.count() > 0) {
                        lock.unlock();
                        std::this_thread::sleep_for(waitDuration);
                        lock.lock();
                        // Recheck after sleep
                        now = std::chrono::steady_clock::now();
                        cutoff = now - std::chrono::seconds(windowSeconds);
                        calls.erase(
                            std::remove_if(calls.begin(), calls.end(), 
                                [&cutoff](const auto& t) { return t < cutoff; }),
                            calls.end()
                        );
                    }
                }
            }
            
            for (int i = 0; i < tokens; ++i) {
                calls.push_back(now);
            }
        }
    };
    
    std::unique_ptr<RateLimiter> rateLimiter_;

    // HTTP helpers
    std::string http_get(const std::string& url, const std::string& authHeader = "");
    std::string http_post(const std::string& url, const std::string& body, const std::string& authHeader = "");
    std::string http_delete(const std::string& url, const std::string& authHeader = "");
    std::string http_put(const std::string& url, const std::string& body, const std::string& authHeader = "");
    
    // Build auth header
    std::string auth_header() const { return token_.empty() ? "" : "Authorization: Bearer " + token_; }
    
    // Handle HTTP response errors
    void handle_http_error(int status, const std::string& body, const std::string& operation);
    
    // Multipart upload helper
    std::string upload_multipart(const std::string& url, const std::vector<std::pair<std::string, std::string>>& files, const std::string& commitMessage);
};

} // namespace hf_transport

#endif // HF_TRANSPORT_STUB_H