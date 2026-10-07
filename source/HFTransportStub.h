//
// HF Stego Transport - C++ HF Hub API Wrapper
//

#ifndef HF_STEGO_TRANSPORT_HF_TRANSPORT_STUB_H
#define HF_STEGO_TRANSPORT_HF_TRANSPORT_STUB_H

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

struct CreateRepoResult {
    std::string repo_id;
    std::string commit_sha;
    std::string url;
};

struct UploadResult {
    std::string commit_sha;
    std::string repo_id;
};

struct DownloadResult {
    std::string commit_sha;
    std::string repo_id;
    std::string local_path;
};

struct RepoInfo {
    std::string repo_id;
    std::string commit_sha;
    bool exists;
};

class HFTransportStub {
public:
    HFTransportStub();
    ~HFTransportStub();

    // Disable copy/move
    HFTransportStub(const HFTransportStub&) = delete;
    HFTransportStub& operator=(const HFTransportStub&) = delete;
    HFTransportStub(HFTransportStub&&) = delete;
    HFTransportStub& operator=(HFTransportStub&&) = delete;

    // Configuration
    void set_token(const std::string& token);
    void set_username(const std::string& username);

    // Repository operations
    CreateRepoResult create_repo(const std::string& repo_id, bool private_repo = true);
    bool repo_exists(const std::string& repo_id);
    RepoInfo get_repo_info(const std::string& repo_id);

    // File operations
    UploadResult upload_folder(const std::string& local_path, const std::string& repo_id, const std::string& commit_message);
    DownloadResult download_repo(const std::string& repo_id, const std::string& local_path, const std::string& revision = "");
    bool delete_repo(const std::string& repo_id);

    // Address handling
    std::string construct_address(const std::string& username, const std::string& repo_name, const std::string& commit_sha = "") const;
    std::optional<std::tuple<std::string, std::string, std::string>> parse_address(const std::string& address) const;

    // Exceptions
    class AuthError : public std::exception {
    public:
        explicit AuthError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    class NotFoundError : public std::exception {
    public:
        explicit NotFoundError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    class RateLimitError : public std::exception {
    public:
        explicit RateLimitError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

    class HFError : public std::exception {
    public:
        explicit HFError(const std::string& msg) : message(msg) {}
        const char* what() const noexcept override { return message.c_str(); }
    private:
        std::string message;
    };

private:
    std::string token_;
    std::string username_;
    std::string api_base_ = "https://huggingface.co/api";

    // Internal HTTP methods
    std::string http_get(const std::string& url, const std::string& auth_token = "");
    std::string http_post(const std::string& url, const std::string& body, const std::string& auth_token = "");
    std::string http_delete(const std::string& url, const std::string& auth_token = "");
    std::string http_put(const std::string& url, const std::string& body, const std::string& auth_token = "");

    // Helper methods
    std::string get_auth_header() const;
    void check_response(const std::string& response, int http_code, const std::string& operation);
    nlohmann::json parse_json(const std::string& response);
};

#endif // HF_STEGO_TRANSPORT_HF_TRANSPORT_STUB_H