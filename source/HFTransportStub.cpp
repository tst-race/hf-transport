//
// HF Stego Transport - C++ HF Hub API Wrapper Implementation
//

#include "HFTransportStub.h"
#include "curlwrap.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static size_t ReadCallback(void* ptr, size_t size, size_t nmemb, void* userp) {
    std::ifstream* stream = (std::ifstream*)userp;
    if (!stream || stream->eof()) return 0;
    stream->read((char*)ptr, size * nmemb);
    return stream->gcount();
}

HFTransportStub::HFTransportStub() : token_(""), username_("") {}

HFTransportStub::~HFTransportStub() {}

void HFTransportStub::set_token(const std::string& token) {
    token_ = token;
}

void HFTransportStub::set_username(const std::string& username) {
    username_ = username;
}

std::string HFTransportStub::get_auth_header() const {
    if (token_.empty()) return "";
    return "Authorization: Bearer " + token_;
}

void HFTransportStub::check_response(const std::string& response, int http_code, const std::string& operation) {
    if (http_code == 401) {
        throw AuthError(operation + ": Authentication failed (401)");
    } else if (http_code == 403) {
        throw AuthError(operation + ": Permission denied (403)");
    } else if (http_code == 404) {
        throw NotFoundError(operation + ": Resource not found (404)");
    } else if (http_code == 429) {
        throw RateLimitError(operation + ": Rate limited (429)");
    } else if (http_code >= 400) {
        throw HFError(operation + ": HTTP " + std::to_string(http_code) + " - " + response);
    }
}

nlohmann::json HFTransportStub::parse_json(const std::string& response) {
    try {
        return nlohmann::json::parse(response);
    } catch (const nlohmann::json::exception& e) {
        throw HFError("JSON parse error: " + std::string(e.what()));
    }
}

std::string HFTransportStub::http_get(const std::string& url, const std::string& auth_token) {
    CurlWrap curl;
    std::string response;

    curl.setopt(CURLOPT_URL, url.c_str());
    curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
    curl.setopt(CURLOPT_WRITEDATA, &response);
    curl.setopt(CURLOPT_FOLLOWLOCATION, 1L);
    curl.setopt(CURLOPT_TIMEOUT, 30L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!auth_token.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token).c_str());
    }
    curl.setopt(CURLOPT_HTTPHEADER, headers);

    curl.perform();

    long http_code = curl.getinfo<long>(CURLINFO_RESPONSE_CODE);
    curl_slist_free_all(headers);

    check_response(response, http_code, "GET " + url);
    return response;
}

std::string HFTransportStub::http_post(const std::string& url, const std::string& body, const std::string& auth_token) {
    CurlWrap curl;
    std::string response;

    curl.setopt(CURLOPT_URL, url.c_str());
    curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
    curl.setopt(CURLOPT_WRITEDATA, &response);
    curl.setopt(CURLOPT_POST, 1L);
    curl.setopt(CURLOPT_POSTFIELDS, body.c_str());
    curl.setopt(CURLOPT_POSTFIELDSIZE, body.size());
    curl.setopt(CURLOPT_FOLLOWLOCATION, 1L);
    curl.setopt(CURLOPT_TIMEOUT, 30L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!auth_token.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token).c_str());
    }
    curl.setopt(CURLOPT_HTTPHEADER, headers);

    curl.perform();

    long http_code = curl.getinfo<long>(CURLINFO_RESPONSE_CODE);
    curl_slist_free_all(headers);

    check_response(response, http_code, "POST " + url);
    return response;
}

std::string HFTransportStub::http_delete(const std::string& url, const std::string& auth_token) {
    CurlWrap curl;
    std::string response;

    curl.setopt(CURLOPT_URL, url.c_str());
    curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
    curl.setopt(CURLOPT_WRITEDATA, &response);
    curl.setopt(CURLOPT_CUSTOMREQUEST, "DELETE");
    curl.setopt(CURLOPT_FOLLOWLOCATION, 1L);
    curl.setopt(CURLOPT_TIMEOUT, 30L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!auth_token.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token).c_str());
    }
    curl.setopt(CURLOPT_HTTPHEADER, headers);

    curl.perform();

    long http_code = curl.getinfo<long>(CURLINFO_RESPONSE_CODE);
    curl_slist_free_all(headers);

    check_response(response, http_code, "DELETE " + url);
    return response;
}

std::string HFTransportStub::http_put(const std::string& url, const std::string& body, const std::string& auth_token) {
    CurlWrap curl;
    std::string response;

    curl.setopt(CURLOPT_URL, url.c_str());
    curl.setopt(CURLOPT_WRITEFUNCTION, WriteCallback);
    curl.setopt(CURLOPT_WRITEDATA, &response);
    curl.setopt(CURLOPT_CUSTOMREQUEST, "PUT");
    curl.setopt(CURLOPT_POSTFIELDS, body.c_str());
    curl.setopt(CURLOPT_POSTFIELDSIZE, body.size());
    curl.setopt(CURLOPT_FOLLOWLOCATION, 1L);
    curl.setopt(CURLOPT_TIMEOUT, 30L);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    if (!auth_token.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + auth_token).c_str());
    }
    curl.setopt(CURLOPT_HTTPHEADER, headers);

    curl.perform();

    long http_code = curl.getinfo<long>(CURLINFO_RESPONSE_CODE);
    curl_slist_free_all(headers);

    check_response(response, http_code, "PUT " + url);
    return response;
}

CreateRepoResult HFTransportStub::create_repo(const std::string& repo_id, bool private_repo) {
    std::string url = api_base_ + "/repos/create";
    nlohmann::json body = {
        {"name", repo_id},
        {"private", private_repo},
        {"exist_ok", false}
    };

    std::string response = http_post(url, body.dump(), token_);
    auto json = parse_json(response);

    CreateRepoResult result;
    result.repo_id = json.value("id", repo_id);
    result.commit_sha = json.value("sha", "");
    result.url = json.value("url", "https://huggingface.co/" + repo_id);
    return result;
}

bool HFTransportStub::repo_exists(const std::string& repo_id) {
    std::string url = api_base_ + "/repos/" + repo_id;
    try {
        std::string response = http_get(url, token_);
        return true;
    } catch (const NotFoundError&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

RepoInfo HFTransportStub::get_repo_info(const std::string& repo_id) {
    std::string url = api_base_ + "/repos/" + repo_id;
    std::string response = http_get(url, token_);
    auto json = parse_json(response);

    RepoInfo info;
    info.repo_id = repo_id;
    info.commit_sha = json.value("sha", "");
    info.exists = true;
    return info;
}

UploadResult HFTransportStub::upload_folder(const std::string& local_path, const std::string& repo_id, const std::string& commit_message) {
    // For simplicity, we'll use the HF Hub API to upload files
    // In a real implementation, we'd use the git-based API or the file upload API
    // Here we use a simplified approach: create a commit with the files

    // First, get the current commit SHA
    RepoInfo info = get_repo_info(repo_id);
    std::string parent_sha = info.commit_sha;

    // Create a commit with the files
    // For now, we'll use a simplified approach - just create a commit via the API
    // Real implementation would use the git LFS or tree-based API

    std::string url = api_base_ + "/repos/" + repo_id + "/commit/main";
    nlohmann::json body = {
        {"commit_message", commit_message},
        {"operations", nlohmann::json::array()}
    };

    // Add all files in the local path
    for (const auto& entry : fs::recursive_directory_iterator(local_path)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            std::string relative_path = fs::relative(entry.path(), local_path).string();
            // Convert to forward slashes for HF
            std::replace(relative_path.begin(), relative_path.end(), '\\', '/');

            body["operations"].push_back({
                {"action", "add"},
                {"path", relative_path},
                {"content", content}
            });
        }
    }

    std::string response = http_post(url, body.dump(), token_);
    auto json = parse_json(response);

    UploadResult result;
    result.repo_id = repo_id;
    result.commit_sha = json.value("commit_oid", json.value("sha", ""));
    return result;
}

DownloadResult HFTransportStub::download_repo(const std::string& repo_id, const std::string& local_path, const std::string& revision) {
    fs::create_directories(local_path);

    // Get repo tree
    std::string rev = revision.empty() ? "main" : revision;
    std::string url = api_base_ + "/repos/" + repo_id + "/tree/" + rev + "?recursive=1";
    std::string response = http_get(url, "");  // Public download, no auth needed
    auto json = parse_json(response);

    std::string commit_sha = json.value("commit_oid", json.value("sha", ""));

    // Download each file
    for (const auto& item : json) {
        if (item.value("type", "") == "file") {
            std::string file_path = item.value("path", "");
            std::string file_url = "https://huggingface.co/" + repo_id + "/resolve/" + rev + "/" + file_path;

            std::string file_response = http_get(file_url, "");

            fs::path out_path = fs::path(local_path) / file_path;
            fs::create_directories(out_path.parent_path());

            std::ofstream out_file(out_path, std::ios::binary);
            out_file.write(file_response.data(), file_response.size());
        }
    }

    DownloadResult result;
    result.repo_id = repo_id;
    result.commit_sha = commit_sha;
    result.local_path = local_path;
    return result;
}

bool HFTransportStub::delete_repo(const std::string& repo_id) {
    std::string url = api_base_ + "/repos/" + repo_id;
    try {
        http_delete(url, token_);
        return true;
    } catch (const NotFoundError&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

std::string HFTransportStub::construct_address(const std::string& username, const std::string& repo_name, const std::string& commit_sha) const {
    std::string addr = "hf://" + username + "/" + repo_name;
    if (!commit_sha.empty()) {
        addr += "@" + commit_sha;
    }
    return addr;
}

std::optional<std::tuple<std::string, std::string, std::string>> HFTransportStub::parse_address(const std::string& address) const {
    if (address.rfind("hf://", 0) != 0) {
        return std::nullopt;
    }

    std::string path = address.substr(5);
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

    return std::make_tuple(username, repo_name, commit_sha);
}