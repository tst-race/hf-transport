#include "HfTransportStub.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <cstdio>
#include <unistd.h>

namespace hf_transport {

using json = nlohmann::json;

namespace fs = std::filesystem;

// Curl write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

HfTransportStub::HfTransportStub(const std::string& token, const std::string& username)
    : token_(token), username_(username), rateLimiter_(std::make_unique<RateLimiter>(rateLimitCalls_, rateLimitWindow_)) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    LOG_METHOD("initialized for user: " + username_);
}

HfTransportStub::~HfTransportStub() {
    curl_global_cleanup();
}

void HfTransportStub::set_rate_limit(int calls, int windowSeconds) {
    rateLimitCalls_ = calls;
    rateLimitWindow_ = windowSeconds;
    rateLimiter_ = std::make_unique<RateLimiter>(calls, windowSeconds);
}

std::string HfTransportStub::http_get(const std::string& url, const std::string& authHeader) {
    rateLimiter_->acquire();
    
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) throw HfError("Failed to initialize curl");
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "User-Agent: racegen-hf-transport/1.0");
    if (!authHeader.empty()) {
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw HfError("CURL error: " + std::string(curl_easy_strerror(res)));
    }
    
    if (httpCode >= 400) {
        handle_http_error(static_cast<int>(httpCode), response, "GET " + url);
    }
    
    return response;
}

std::string HfTransportStub::http_post(const std::string& url, const std::string& body, const std::string& authHeader) {
    logDebug("http_post: acquiring rate limiter for " + url);
    rateLimiter_->acquire();
    logDebug("http_post: rate limiter acquired");
    
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) throw HfError("Failed to initialize curl");
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: racegen-hf-transport/1.0");
    if (!authHeader.empty()) {
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    logDebug("http_post: starting curl_easy_perform for " + url);
    CURLcode res = curl_easy_perform(curl);
    logDebug("http_post: curl_easy_perform returned: " + std::to_string(res));
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    logDebug("http_post: HTTP code: " + std::to_string(httpCode));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw HfError("CURL error: " + std::string(curl_easy_strerror(res)));
    }
    
    if (httpCode >= 400) {
        handle_http_error(static_cast<int>(httpCode), response, "POST " + url);
    }
    
    return response;
}

std::string HfTransportStub::http_delete(const std::string& url, const std::string& authHeader) {
    rateLimiter_->acquire();
    
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) throw HfError("Failed to initialize curl");
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "User-Agent: racegen-hf-transport/1.0");
    if (!authHeader.empty()) {
        headers = curl_slist_append(headers, authHeader.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw HfError("CURL error: " + std::string(curl_easy_strerror(res)));
    }
    
    if (httpCode >= 400) {
        handle_http_error(static_cast<int>(httpCode), response, "DELETE " + url);
    }
    
    return response;
}

void HfTransportStub::handle_http_error(int status, const std::string& body, const std::string& operation) {
    LOG_METHOD("HF API error " + std::to_string(status) + " for " + operation + ": " + body);
    
    if (status == 401) {
        throw AuthError("Authentication failed (" + operation + "): token invalid or expired");
    } else if (status == 403) {
        throw AuthError("Forbidden (" + operation + "): insufficient permissions");
    } else if (status == 404) {
        throw NotFoundError("Resource not found (" + operation + ")");
    } else if (status == 409) {
        throw ConflictError("Conflict (" + operation + "): repository already exists");
    } else if (status == 429) {
        int retryAfter = 60;
        throw RateLimitError("Rate limited (" + operation + ")", retryAfter);
    } else if (status >= 500) {
        throw ServerError("HF Hub server error (" + operation + "): " + std::to_string(status));
    } else {
        throw HfError("Unexpected error (" + operation + "): " + std::to_string(status) + " - " + body);
    }
}

RepoInfo HfTransportStub::create_repo(const std::string& repoName, bool privateRepo, bool existOk) {
    // Write to file for debugging
    FILE* f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: ENTRY\n");
        fclose(f);
    }
    
    // Sleep to see if function is called
    sleep(5);
    
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: AFTER SLEEP\n");
        fclose(f);
    }
    
    // TRACE_METHOD();
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: repoName=%s\n", repoName.c_str());
        fclose(f);
    }
    
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: building request\n");
        fclose(f);
    }
    std::string repoId = username_ + "/" + repoName;
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: repoId=%s\n", repoId.c_str());
        fclose(f);
    }
    std::string url = apiBase_ + "/repos/create";
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: url=%s\n", url.c_str());
        fclose(f);
    }
    
    json body;
    body["name"] = repoName;
    body["private"] = privateRepo;
    body["exist_ok"] = existOk;
    body["type"] = "model";
    std::string bodyStr = body.dump();
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: body=%s\n", bodyStr.c_str());
        fclose(f);
    }
    
    std::string auth = auth_header();
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: auth len=%zu\n", auth.length());
        fclose(f);
    }
    
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: calling http_post\n");
        fclose(f);
    }
    std::string response = http_post(url, bodyStr, auth);
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: http_post returned, len=%zu\n", response.length());
        fclose(f);
    }
    json j = json::parse(response);
    
    RepoInfo info;
    info.repoId = repoId;
    info.author = username_;
    info.name = repoName;
    info.privateRepo = privateRepo;
    info.commitSha = j.value("sha", "");
    info.url = "https://huggingface.co/" + repoId;
    
    f = fopen("/tmp/hf_debug.log", "a");
    if (f) {
        fprintf(f, "create_repo: done, commit=%s\n", info.commitSha.c_str());
        fclose(f);
    }
    return info;
}

bool HfTransportStub::delete_repo(const std::string& repoId) {
    TRACE_METHOD();
    
    std::string url = apiBase_ + "/repos/delete/" + repoId;
    http_delete(url, auth_header());
    
    LOG_METHOD("Deleted repo: " + repoId);
    return true;
}

bool HfTransportStub::repo_exists(const std::string& repoId) {
    TRACE_METHOD();
    
    // First try REST API
    try {
        std::string url = apiBase_ + "/repos/" + repoId;
        http_get(url, auth_header());
        return true;
    } catch (const NotFoundError&) {
        // Fall back to git ls-remote (more reliable for recently created repos)
    } catch (const AuthError&) {
        throw;
    } catch (...) {
        // Fall through to git check
    }
    
    // Fallback: use git ls-remote to check if repo exists
    try {
        std::string repoUrl = "https://huggingface.co/" + repoId;
        std::string cmd = "git ls-remote " + repoUrl + " HEAD 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            LOG_METHOD("git ls-remote failed to start");
            return false;
        }
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        int status = pclose(pipe);
        if (status == 0 && !result.empty()) {
            LOG_METHOD("git ls-remote confirmed repo exists: " + repoId);
            return true;
        }
    } catch (...) {
        // Ignore git check errors
    }
    
    return false;
}

RepoInfo HfTransportStub::get_repo_info(const std::string& repoId) {
    TRACE_METHOD();
    
    std::string url = apiBase_ + "/repos/" + repoId;
    std::string response = http_get(url, auth_header());
    json j = json::parse(response);
    
    RepoInfo info;
    info.repoId = repoId;
    size_t slashPos = repoId.find('/');
    info.author = (slashPos != std::string::npos) ? repoId.substr(0, slashPos) : "unknown";
    info.name = (slashPos != std::string::npos) ? repoId.substr(slashPos + 1) : repoId;
    info.privateRepo = j.value("private", false);
    info.commitSha = j.value("sha", "");
    info.url = "https://huggingface.co/" + repoId;
    
    return info;
}

UploadResult HfTransportStub::upload_folder(const std::string& localPath, const std::string& repoId, const std::string& commitMessage) {
    TRACE_METHOD();
    
    // Collect files to upload
    std::vector<std::pair<std::string, std::string>> files;
    for (const auto& entry : fs::recursive_directory_iterator(localPath)) {
        if (entry.is_regular_file()) {
            std::string relPath = fs::relative(entry.path(), localPath).string();
            std::string absPath = entry.path().string();
            files.emplace_back(relPath, absPath);
        }
    }
    
    if (files.empty()) {
        throw HfError("No files to upload in " + localPath);
    }
    
    // Upload via multipart
    std::string url = apiBase_ + "/upload/large/" + repoId + "?type=model";
    std::string response = upload_multipart(url, files, commitMessage);
    json j = json::parse(response);
    
    UploadResult result;
    result.repoId = repoId;
    result.commitSha = j.value("oid", "");
    result.commitUrl = "https://huggingface.co/" + repoId + "/commit/" + result.commitSha;
    result.filesUploaded.reserve(files.size());
    for (const auto& f : files) result.filesUploaded.push_back(f.first);
    
    LOG_METHOD("Uploaded " + std::to_string(files.size()) + " files to " + repoId + " (commit: " + result.commitSha + ")");
    return result;
}

std::string HfTransportStub::upload_multipart(const std::string& url, const std::vector<std::pair<std::string, std::string>>& files, const std::string& commitMessage) {
    rateLimiter_->acquire(static_cast<int>(files.size()));
    
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) throw HfError("Failed to initialize curl");
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "User-Agent: racegen-hf-transport/1.0");
    if (!token_.empty()) {
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
    }
    
    curl_mime* mime = curl_mime_init(curl);
    
    // Add commit message
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, "commit_message");
    curl_mime_data(part, commitMessage.c_str(), CURL_ZERO_TERMINATED);
    
    // Add files
    for (const auto& [relPath, absPath] : files) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filename(part, relPath.c_str());
        curl_mime_filedata(part, absPath.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        throw HfError("CURL error: " + std::string(curl_easy_strerror(res)));
    }
    
    if (httpCode >= 400) {
        handle_http_error(static_cast<int>(httpCode), response, "UPLOAD " + url);
    }
    
    return response;
}

DownloadResult HfTransportStub::download_repo(const std::string& repoId, const std::string& localDir, const std::string& revision) {
    TRACE_METHOD();
    
    // Create local directory
    fs::create_directories(localDir);
    
    // Use git to clone at specific revision
    std::string repoUrl = "https://huggingface.co/" + repoId;
    std::string cmd = "git clone --depth 1 --branch " + revision + " " + repoUrl + " " + localDir + " 2>&1";
    
    LOG_METHOD("Executing: " + cmd);
    int sysResult = std::system(cmd.c_str());
    
    if (sysResult != 0) {
        // Try without branch (maybe it's a commit SHA)
        cmd = "git clone " + repoUrl + " " + localDir + " 2>&1 && cd " + localDir + " && git checkout " + revision + " 2>&1";
        LOG_METHOD("Retrying: " + cmd);
        sysResult = std::system(cmd.c_str());
        if (sysResult != 0) {
            throw NotFoundError("Failed to clone repo " + repoId + " at revision " + revision);
        }
    }
    
    // Get commit SHA
    std::string shaCmd = "cd " + localDir + " && git rev-parse HEAD 2>&1";
    FILE* pipe = popen(shaCmd.c_str(), "r");
    if (!pipe) throw HfError("Failed to get commit SHA");
    char buffer[128];
    std::string commitSha;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        commitSha += buffer;
    }
    pclose(pipe);
    commitSha.erase(commitSha.find_last_not_of(" \n\r\t") + 1);
    
    // List downloaded files
    std::vector<std::string> downloadedFiles;
    for (const auto& entry : fs::recursive_directory_iterator(localDir)) {
        if (entry.is_regular_file()) {
            std::string relPath = fs::relative(entry.path(), localDir).string();
            // Skip .git directory
            if (relPath.find(".git/") == std::string::npos) {
                downloadedFiles.push_back(relPath);
            }
        }
    }
    
    DownloadResult result;
    result.localPath = localDir;
    result.repoId = repoId;
    result.commitSha = commitSha;
    result.filesDownloaded = std::move(downloadedFiles);
    
    LOG_METHOD("Downloaded repo: " + repoId + " at " + commitSha + " (" + std::to_string(result.filesDownloaded.size()) + " files)");
    return result;
}

std::vector<RepoInfo> HfTransportStub::list_user_repos(const std::string& prefix) {
    TRACE_METHOD();
    
    std::string url = apiBase_ + "/models?author=" + username_ + "&limit=1000";
    std::string response = http_get(url, auth_header());
    json repos = json::parse(response);
    
    std::vector<RepoInfo> results;
    for (const auto& repo : repos) {
        std::string repoId = repo.value("id", "");
        if (repoId.find(username_ + "/" + prefix) != std::string::npos) {
            results.push_back(get_repo_info(repoId));
        }
    }
    
    return results;
}

RepoInfo HfTransportStub::resolve_address(const std::string& hfAddress) {
    TRACE_METHOD();
    
    // Parse hf://username/repo@rev
    if (hfAddress.rfind("hf://", 0) != 0) {
        throw HfError("Invalid address format: " + hfAddress);
    }
    
    std::string path = hfAddress.substr(5);
    size_t atPos = path.find('@');
    std::string repoId = (atPos != std::string::npos) ? path.substr(0, atPos) : path;
    
    return get_repo_info(repoId);
}

std::string HfTransportStub::construct_address(const std::string& repoId, const std::string& commitSha) {
    TRACE_METHOD();
    
    if (username_.empty()) {
        throw HfError("Username required to construct address");
    }
    
    std::string address = "hf://" + username_ + "/" + repoId;
    if (!commitSha.empty() && commitSha != "main") {
        address += "@" + commitSha;
    }
    return address;
}

std::vector<CommitInfo> HfTransportStub::list_commits(const std::string& repoId, const std::string& sinceSha) {
    TRACE_METHOD();
    
    // Try HF Hub REST API first
    try {
        std::string url = apiBase_ + "/repos/" + repoId + "/commits?limit=100";
        if (!sinceSha.empty()) {
            url += "&since=" + sinceSha;
        }
        
        std::string response = http_get(url, auth_header());
        json commits = json::parse(response);
        
        std::vector<CommitInfo> results;
        for (const auto& commit : commits) {
            CommitInfo info;
            info.sha = commit.value("oid", "");
            info.message = commit.value("title", "");
            info.timestamp = commit.value("date", "");
            results.push_back(info);
        }
        
        return results;
    } catch (const NotFoundError&) {
        // Fall back to git log
    } catch (const AuthError&) {
        throw;
    } catch (...) {
        // Fall through to git check
    }
    
    // Fallback: use git log to get commit history
    std::vector<CommitInfo> results;
    try {
        std::string safeRepoId = repoId;
        std::replace(safeRepoId.begin(), safeRepoId.end(), '/', '_');
        std::string tempDir = "/tmp/hf_commits_" + safeRepoId;
        fs::remove_all(tempDir);
        fs::create_directories(tempDir);
        
        std::string repoUrl = "https://" + username_ + ":" + token_ + "@huggingface.co/" + repoId;
        std::string cloneCmd = "git clone " + repoUrl + " " + tempDir + " 2>&1";
        LOG_METHOD("Executing: " + cloneCmd);
        int result = std::system(cloneCmd.c_str());
        if (result != 0) {
            LOG_METHOD("git clone failed for list_commits");
            return results;
        }
        
        // Use git log to get commits
        std::string logCmd = "cd " + tempDir + " && git log --oneline --pretty=format:\"%H|%s|%ci\" 2>&1";
        FILE* pipe = popen(logCmd.c_str(), "r");
        if (!pipe) {
            LOG_METHOD("git log failed to start");
            fs::remove_all(tempDir);
            return results;
        }
        
        char buffer[512];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line = buffer;
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            if (line.empty()) continue;
            
            // Parse: sha|message|timestamp
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                std::string sha = line.substr(0, pos1);
                std::string message = line.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string timestamp = line.substr(pos2 + 1);
                
                // If sinceSha is provided, stop when we reach it
                if (!sinceSha.empty() && sha == sinceSha) {
                    break;
                }
                
                CommitInfo info;
                info.sha = sha;
                info.message = message;
                info.timestamp = timestamp;
                results.push_back(info);
            }
        }
        pclose(pipe);
        fs::remove_all(tempDir);
        
        LOG_METHOD("git log returned " + std::to_string(results.size()) + " commits for " + repoId);
        return results;
    } catch (...) {
        LOG_METHOD("git log fallback failed");
        return results;
    }
}

// Create initial commit with a placeholder file using git CLI (more reliable than API)
std::string HfTransportStub::create_initial_commit(const std::string& repoId, const std::string& commitMessage) {
    TRACE_METHOD();
    
    // Create a temporary directory for git operations
    std::string safeRepoId = repoId;
    std::replace(safeRepoId.begin(), safeRepoId.end(), '/', '_');
    std::string tempDir = "/tmp/hf_init_" + safeRepoId;
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);
    
    try {
        // Clone the repo (will be empty initially)
        std::string repoUrl = "https://huggingface.co/" + repoId;
        std::string cloneCmd = "git clone " + repoUrl + " " + tempDir + " 2>&1";
        
        LOG_METHOD("Executing: " + cloneCmd);
        int result = std::system(cloneCmd.c_str());
        if (result != 0) {
            // Repo might be empty (no commits yet), try init instead
            LOG_METHOD("Clone failed, trying git init");
            fs::remove_all(tempDir);
            fs::create_directories(tempDir);
            
            std::string initCmd = "cd " + tempDir + " && git init && git remote add origin " + repoUrl + " 2>&1";
            LOG_METHOD("Executing: " + initCmd);
            result = std::system(initCmd.c_str());
            if (result != 0) {
                throw HfError("Failed to initialize git repo");
            }
        }
        
        // Create placeholder file
        std::string placeholderPath = tempDir + "/placeholder.txt";
        std::ofstream placeholder(placeholderPath);
        placeholder << "HF Transport initialized\n";
        placeholder.close();
        
        // Configure git user
        std::string configCmd = "cd " + tempDir + " && git config user.email \"racegen@hf-transport\" && git config user.name \"RaceGen HF Transport\" 2>&1";
        std::system(configCmd.c_str());
        
        // Add and commit
        std::string addCmd = "cd " + tempDir + " && git add placeholder.txt 2>&1";
        std::system(addCmd.c_str());
        
        std::string commitCmd = "cd " + tempDir + " && git commit -m \"" + commitMessage + "\" 2>&1";
        result = std::system(commitCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to create commit");
        }
        
        // Push to HF Hub (requires authentication)
        // Use token in URL for authentication
        std::string pushUrl = "https://" + username_ + ":" + token_ + "@huggingface.co/" + repoId;
        std::string pushCmd = "cd " + tempDir + " && git push " + pushUrl + " HEAD:main 2>&1";
        LOG_METHOD("Executing: git push to " + repoId);
        result = std::system(pushCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to push initial commit");
        }
        
        // Get commit SHA
        std::string shaCmd = "cd " + tempDir + " && git rev-parse HEAD 2>&1";
        FILE* pipe = popen(shaCmd.c_str(), "r");
        if (!pipe) throw HfError("Failed to get commit SHA");
        char buffer[128];
        std::string commitSha;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            commitSha += buffer;
        }
        pclose(pipe);
        
        // Trim whitespace
        commitSha.erase(std::remove_if(commitSha.begin(), commitSha.end(), ::isspace), commitSha.end());
        
        if (commitSha.empty()) {
            throw HfError("Failed to get commit SHA after push");
        }
        
        LOG_METHOD("Created initial commit for " + repoId + ": " + commitSha);
        return commitSha;
        
    } catch (...) {
        fs::remove_all(tempDir);
        throw;
    }
}

// Commit content files via git (more reliable than large file upload API)
std::string HfTransportStub::commit_content(const std::string& repoId, const std::string& localPath, const std::string& commitMessage) {
    TRACE_METHOD();
    
    // Create a temporary directory for git operations
    std::string safeRepoId = repoId;
    std::replace(safeRepoId.begin(), safeRepoId.end(), '/', '_');
    std::string tempDir = "/tmp/hf_commit_" + safeRepoId + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);
    
    try {
        // Clone the repo (with retry for HF Hub eventual consistency)
        std::string repoUrl = "https://huggingface.co/" + repoId;
        std::string cloneCmd = "git clone " + repoUrl + " " + tempDir + " 2>&1";
        
        const int maxCloneRetries = 5;
        int result = -1;
        for (int attempt = 1; attempt <= maxCloneRetries; ++attempt) {
            LOG_METHOD("Executing (attempt " + std::to_string(attempt) + "/" + std::to_string(maxCloneRetries) + "): " + cloneCmd);
            result = std::system(cloneCmd.c_str());
            if (result == 0) {
                break;  // Success
            }
            if (attempt < maxCloneRetries) {
                LOG_METHOD("Clone failed, waiting 2s before retry...");
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        if (result != 0) {
            throw HfError("Failed to clone repo for commit after " + std::to_string(maxCloneRetries) + " attempts");
        }
        
        // Copy content files to repo
        std::string copyCmd = "cp -r " + localPath + "/* " + tempDir + "/ 2>&1";
        result = std::system(copyCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to copy content files");
        }
        
        // Configure git user
        std::string configCmd = "cd " + tempDir + " && git config user.email \"racegen@hf-transport\" && git config user.name \"RaceGen HF Transport\" 2>&1";
        std::system(configCmd.c_str());
        
        // Add all files
        std::string addCmd = "cd " + tempDir + " && git add . 2>&1";
        result = std::system(addCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to add files to git");
        }
        
        // Commit
        std::string commitCmd = "cd " + tempDir + " && git commit -m \"" + commitMessage + "\" 2>&1";
        result = std::system(commitCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to create commit");
        }
        
        // Push to HF Hub (requires authentication)
        std::string pushUrl = "https://" + username_ + ":" + token_ + "@huggingface.co/" + repoId;
        std::string pushCmd = "cd " + tempDir + " && git push " + pushUrl + " HEAD:main 2>&1";
        LOG_METHOD("Executing: git push to " + repoId);
        result = std::system(pushCmd.c_str());
        if (result != 0) {
            throw HfError("Failed to push commit");
        }
        
        // Get commit SHA
        std::string shaCmd = "cd " + tempDir + " && git rev-parse HEAD 2>&1";
        FILE* pipe = popen(shaCmd.c_str(), "r");
        if (!pipe) throw HfError("Failed to get commit SHA");
        char buffer[128];
        std::string commitSha;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            commitSha += buffer;
        }
        pclose(pipe);
        
        // Trim whitespace
        commitSha.erase(std::remove_if(commitSha.begin(), commitSha.end(), ::isspace), commitSha.end());
        
        if (commitSha.empty()) {
            throw HfError("Failed to get commit SHA after push");
        }
        
        LOG_METHOD("Committed content for " + repoId + ": " + commitSha);
        return commitSha;
        
    } catch (...) {
        fs::remove_all(tempDir);
        throw;
    }
}

} // namespace hf_transport