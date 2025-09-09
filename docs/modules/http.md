# HTTP客户端模块使用指南

HTTP客户端模块提供了基于libcurl的HTTP/HTTPS通信功能，支持JSON数据传输、文件上传下载、认证等功能，是LinxSDK中与服务器进行RESTful API交互的核心组件。

## 模块概述

### 核心类

- **HttpClient**: HTTP客户端实现类

### 主要功能

- 支持HTTP/HTTPS协议
- JSON数据的POST请求
- 文件上传和下载
- 自定义HTTP头部
- 认证支持（Bearer Token等）
- 超时控制
- 错误处理和重试机制
- 响应数据处理

## API参考

### HttpClient 类

```cpp
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    // POST JSON数据
    std::string PostJson(const std::string& url, const std::string& json_data);
    
    // 上传文件
    std::string UploadFile(const std::string& url, const std::string& file_path);
    
    // 设置HTTP头部
    void SetHeaders(const std::map<std::string, std::string>& headers);
    
    // 设置超时时间
    void SetTimeout(long timeout_seconds);
    
    // 设置用户代理
    void SetUserAgent(const std::string& user_agent);
    
private:
    CURL* curl_;
    struct curl_slist* headers_;
    std::map<std::string, std::string> custom_headers_;
    long timeout_;
    std::string user_agent_;
};
```

### 响应处理回调

```cpp
// 响应数据写入回调
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response);

// 上传进度回调
static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                           curl_off_t ultotal, curl_off_t ulnow);
```

## 使用示例

### 基本HTTP请求

```cpp
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include <iostream>

using namespace linx;

int main() {
    try {
        // 1. 创建HTTP客户端
        HttpClient client;
        
        // 2. 设置超时时间（30秒）
        client.SetTimeout(30);
        
        // 3. 设置用户代理
        client.SetUserAgent("LinxSDK/1.0");
        
        // 4. 设置认证头部
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer your-access-token";
        headers["Content-Type"] = "application/json";
        client.SetHeaders(headers);
        
        // 5. 准备JSON数据
        json request_data = {
            {"message", "Hello, World!"},
            {"timestamp", std::time(nullptr)},
            {"user_id", "user123"}
        };
        
        // 6. 发送POST请求
        std::string response = client.PostJson(
            "https://api.example.com/messages", 
            request_data.dump()
        );
        
        // 7. 解析响应
        if (!response.empty()) {
            json response_json = json::parse(response);
            INFO("响应: {}", response_json.dump(2));
            
            if (response_json.contains("status") && 
                response_json["status"] == "success") {
                INFO("请求成功");
            } else {
                ERROR("请求失败: {}", response_json["error"].get<std::string>());
            }
        } else {
            ERROR("请求失败，无响应数据");
        }
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 文件上传示例

```cpp
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include <fstream>

using namespace linx;

class FileUploader {
private:
    HttpClient client;
    std::string api_base_url;
    std::string access_token;
    
public:
    FileUploader(const std::string& base_url, const std::string& token)
        : api_base_url(base_url), access_token(token) {
        setupClient();
    }
    
    void setupClient() {
        // 设置超时时间（5分钟，适合大文件上传）
        client.SetTimeout(300);
        
        // 设置认证头部
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer " + access_token;
        client.SetHeaders(headers);
    }
    
    bool uploadAudioFile(const std::string& file_path, const std::string& session_id) {
        try {
            // 1. 检查文件是否存在
            std::ifstream file(file_path);
            if (!file.good()) {
                ERROR("文件不存在: {}", file_path);
                return false;
            }
            
            // 2. 构建上传URL
            std::string upload_url = api_base_url + "/upload/audio?session_id=" + session_id;
            
            INFO("开始上传文件: {} 到 {}", file_path, upload_url);
            
            // 3. 上传文件
            std::string response = client.UploadFile(upload_url, file_path);
            
            // 4. 解析响应
            if (!response.empty()) {
                json response_json = json::parse(response);
                
                if (response_json.contains("file_id")) {
                    std::string file_id = response_json["file_id"];
                    INFO("文件上传成功，文件ID: {}", file_id);
                    return true;
                } else {
                    ERROR("上传失败: {}", response_json["error"].get<std::string>());
                    return false;
                }
            } else {
                ERROR("上传失败，无响应数据");
                return false;
            }
            
        } catch (const std::exception& e) {
            ERROR("上传过程中出错: {}", e.what());
            return false;
        }
    }
    
    bool uploadMultipleFiles(const std::vector<std::string>& file_paths, 
                           const std::string& session_id) {
        bool all_success = true;
        
        for (const auto& file_path : file_paths) {
            if (!uploadAudioFile(file_path, session_id)) {
                all_success = false;
                ERROR("文件上传失败: {}", file_path);
            } else {
                INFO("文件上传成功: {}", file_path);
            }
            
            // 上传间隔，避免服务器压力
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        return all_success;
    }
};

int main() {
    try {
        FileUploader uploader("https://api.example.com", "your-access-token");
        
        // 上传单个文件
        if (uploader.uploadAudioFile("recording.wav", "session123")) {
            INFO("单文件上传成功");
        }
        
        // 上传多个文件
        std::vector<std::string> files = {
            "audio1.wav",
            "audio2.wav",
            "audio3.wav"
        };
        
        if (uploader.uploadMultipleFiles(files, "session123")) {
            INFO("多文件上传全部成功");
        }
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### RESTful API客户端

```cpp
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include <thread>
#include <chrono>

using namespace linx;

class ApiClient {
private:
    HttpClient client;
    std::string base_url;
    std::string access_token;
    std::string refresh_token;
    std::chrono::system_clock::time_point token_expires_at;
    
public:
    ApiClient(const std::string& url) : base_url(url) {
        client.SetTimeout(30);
        client.SetUserAgent("LinxSDK-ApiClient/1.0");
    }
    
    bool authenticate(const std::string& username, const std::string& password) {
        try {
            json auth_data = {
                {"username", username},
                {"password", password},
                {"grant_type", "password"}
            };
            
            std::string response = client.PostJson(
                base_url + "/auth/token", 
                auth_data.dump()
            );
            
            if (!response.empty()) {
                json auth_response = json::parse(response);
                
                if (auth_response.contains("access_token")) {
                    access_token = auth_response["access_token"];
                    refresh_token = auth_response["refresh_token"];
                    
                    int expires_in = auth_response["expires_in"];
                    token_expires_at = std::chrono::system_clock::now() + 
                                     std::chrono::seconds(expires_in);
                    
                    updateAuthHeaders();
                    
                    INFO("认证成功，令牌有效期: {} 秒", expires_in);
                    return true;
                } else {
                    ERROR("认证失败: {}", auth_response["error"].get<std::string>());
                    return false;
                }
            }
            
        } catch (const std::exception& e) {
            ERROR("认证过程中出错: {}", e.what());
        }
        
        return false;
    }
    
    bool refreshAccessToken() {
        try {
            json refresh_data = {
                {"refresh_token", refresh_token},
                {"grant_type", "refresh_token"}
            };
            
            std::string response = client.PostJson(
                base_url + "/auth/refresh", 
                refresh_data.dump()
            );
            
            if (!response.empty()) {
                json refresh_response = json::parse(response);
                
                if (refresh_response.contains("access_token")) {
                    access_token = refresh_response["access_token"];
                    
                    int expires_in = refresh_response["expires_in"];
                    token_expires_at = std::chrono::system_clock::now() + 
                                     std::chrono::seconds(expires_in);
                    
                    updateAuthHeaders();
                    
                    INFO("令牌刷新成功");
                    return true;
                }
            }
            
        } catch (const std::exception& e) {
            ERROR("令牌刷新失败: {}", e.what());
        }
        
        return false;
    }
    
    void updateAuthHeaders() {
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer " + access_token;
        headers["Content-Type"] = "application/json";
        client.SetHeaders(headers);
    }
    
    bool ensureValidToken() {
        auto now = std::chrono::system_clock::now();
        
        // 如果令牌在5分钟内过期，尝试刷新
        if (now + std::chrono::minutes(5) >= token_expires_at) {
            INFO("令牌即将过期，尝试刷新");
            return refreshAccessToken();
        }
        
        return true;
    }
    
    json get(const std::string& endpoint) {
        if (!ensureValidToken()) {
            throw std::runtime_error("无法获取有效令牌");
        }
        
        // 对于GET请求，我们需要扩展HttpClient类
        // 这里简化处理，实际应该添加GET方法
        throw std::runtime_error("GET方法需要在HttpClient中实现");
    }
    
    json post(const std::string& endpoint, const json& data) {
        if (!ensureValidToken()) {
            throw std::runtime_error("无法获取有效令牌");
        }
        
        std::string response = client.PostJson(
            base_url + endpoint, 
            data.dump()
        );
        
        if (!response.empty()) {
            return json::parse(response);
        }
        
        return json::object();
    }
    
    // 创建语音会话
    std::string createVoiceSession(const json& session_config) {
        try {
            json response = post("/sessions/voice", session_config);
            
            if (response.contains("session_id")) {
                std::string session_id = response["session_id"];
                INFO("语音会话创建成功: {}", session_id);
                return session_id;
            } else {
                ERROR("会话创建失败: {}", response["error"].get<std::string>());
            }
            
        } catch (const std::exception& e) {
            ERROR("创建会话时出错: {}", e.what());
        }
        
        return "";
    }
    
    // 获取会话状态
    json getSessionStatus(const std::string& session_id) {
        try {
            // 这里需要GET方法，简化处理
            json query_data = {{"session_id", session_id}};
            return post("/sessions/status", query_data);
            
        } catch (const std::exception& e) {
            ERROR("获取会话状态时出错: {}", e.what());
            return json::object();
        }
    }
    
    // 结束会话
    bool endSession(const std::string& session_id) {
        try {
            json end_data = {{"session_id", session_id}};
            json response = post("/sessions/end", end_data);
            
            if (response.contains("status") && response["status"] == "ended") {
                INFO("会话已结束: {}", session_id);
                return true;
            }
            
        } catch (const std::exception& e) {
            ERROR("结束会话时出错: {}", e.what());
        }
        
        return false;
    }
};

int main() {
    try {
        ApiClient api("https://api.example.com");
        
        // 1. 认证
        if (!api.authenticate("username", "password")) {
            ERROR("认证失败");
            return -1;
        }
        
        // 2. 创建语音会话
        json session_config = {
            {"mode", "voice_chat"},
            {"language", "zh-CN"},
            {"audio_format", "opus"},
            {"sample_rate", 16000}
        };
        
        std::string session_id = api.createVoiceSession(session_config);
        if (session_id.empty()) {
            ERROR("无法创建会话");
            return -1;
        }
        
        // 3. 模拟会话使用
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // 4. 检查会话状态
        json status = api.getSessionStatus(session_id);
        INFO("会话状态: {}", status.dump(2));
        
        // 5. 结束会话
        api.endSession(session_id);
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

### 扩展的HTTP客户端

```cpp
#include "HttpClient.h"
#include "Json.h"
#include "Log.h"
#include <curl/curl.h>
#include <fstream>

using namespace linx;

class ExtendedHttpClient : public HttpClient {
private:
    struct ProgressData {
        std::function<void(double)> callback;
        std::chrono::system_clock::time_point start_time;
    };
    
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                               curl_off_t ultotal, curl_off_t ulnow) {
        ProgressData* progress = static_cast<ProgressData*>(clientp);
        
        if (progress && progress->callback) {
            double percentage = 0.0;
            
            if (ultotal > 0) {
                percentage = (double)ulnow / ultotal * 100.0;
            } else if (dltotal > 0) {
                percentage = (double)dlnow / dltotal * 100.0;
            }
            
            progress->callback(percentage);
        }
        
        return 0; // 继续传输
    }
    
    static size_t writeToFile(void* contents, size_t size, size_t nmemb, FILE* file) {
        return fwrite(contents, size, nmemb, file);
    }
    
public:
    // GET请求
    std::string Get(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("无法初始化CURL");
        }
        
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("GET请求失败: " + std::string(curl_easy_strerror(res)));
        }
        
        if (response_code >= 400) {
            throw std::runtime_error("HTTP错误: " + std::to_string(response_code));
        }
        
        return response;
    }
    
    // 带进度回调的文件上传
    std::string UploadFileWithProgress(const std::string& url, 
                                     const std::string& file_path,
                                     std::function<void(double)> progress_callback) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("无法初始化CURL");
        }
        
        FILE* file = fopen(file_path.c_str(), "rb");
        if (!file) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("无法打开文件: " + file_path);
        }
        
        // 获取文件大小
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        std::string response;
        ProgressData progress_data;
        progress_data.callback = progress_callback;
        progress_data.start_time = std::chrono::system_clock::now();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, file);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5分钟超时
        
        CURLcode res = curl_easy_perform(curl);
        
        fclose(file);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("上传失败: " + std::string(curl_easy_strerror(res)));
        }
        
        if (response_code >= 400) {
            throw std::runtime_error("HTTP错误: " + std::to_string(response_code));
        }
        
        return response;
    }
    
    // 下载文件
    bool DownloadFile(const std::string& url, const std::string& file_path,
                     std::function<void(double)> progress_callback = nullptr) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            ERROR("无法初始化CURL");
            return false;
        }
        
        FILE* file = fopen(file_path.c_str(), "wb");
        if (!file) {
            ERROR("无法创建文件: {}", file_path);
            curl_easy_cleanup(curl);
            return false;
        }
        
        ProgressData progress_data;
        progress_data.callback = progress_callback;
        progress_data.start_time = std::chrono::system_clock::now();
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
        
        if (progress_callback) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        }
        
        CURLcode res = curl_easy_perform(curl);
        
        fclose(file);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            ERROR("下载失败: {}", curl_easy_strerror(res));
            return false;
        }
        
        if (response_code >= 400) {
            ERROR("HTTP错误: {}", response_code);
            return false;
        }
        
        return true;
    }
    
    // PUT请求
    std::string Put(const std::string& url, const std::string& data) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("无法初始化CURL");
        }
        
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("PUT请求失败: " + std::string(curl_easy_strerror(res)));
        }
        
        if (response_code >= 400) {
            throw std::runtime_error("HTTP错误: " + std::to_string(response_code));
        }
        
        return response;
    }
    
    // DELETE请求
    std::string Delete(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("无法初始化CURL");
        }
        
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("DELETE请求失败: " + std::string(curl_easy_strerror(res)));
        }
        
        if (response_code >= 400) {
            throw std::runtime_error("HTTP错误: " + std::to_string(response_code));
        }
        
        return response;
    }
};

// 使用示例
int main() {
    try {
        ExtendedHttpClient client;
        
        // 设置认证头部
        std::map<std::string, std::string> headers;
        headers["Authorization"] = "Bearer your-token";
        client.SetHeaders(headers);
        
        // GET请求
        std::string get_response = client.Get("https://api.example.com/data");
        INFO("GET响应: {}", get_response);
        
        // 带进度的文件上传
        client.UploadFileWithProgress(
            "https://api.example.com/upload",
            "large_file.wav",
            [](double progress) {
                INFO("上传进度: {:.1f}%", progress);
            }
        );
        
        // 文件下载
        client.DownloadFile(
            "https://api.example.com/download/file.zip",
            "downloaded_file.zip",
            [](double progress) {
                INFO("下载进度: {:.1f}%", progress);
            }
        );
        
        // PUT请求
        json put_data = {{"status", "updated"}};
        std::string put_response = client.Put(
            "https://api.example.com/resource/123",
            put_data.dump()
        );
        
        // DELETE请求
        std::string delete_response = client.Delete("https://api.example.com/resource/123");
        
    } catch (const std::exception& e) {
        ERROR("错误: {}", e.what());
        return -1;
    }
    
    return 0;
}
```

## 错误处理和重试机制

### 重试客户端

```cpp
class RetryHttpClient {
private:
    ExtendedHttpClient client;
    int max_retries;
    std::chrono::milliseconds base_delay;
    
public:
    RetryHttpClient(int retries = 3, std::chrono::milliseconds delay = std::chrono::milliseconds(1000))
        : max_retries(retries), base_delay(delay) {}
    
    template<typename Func>
    auto executeWithRetry(Func&& func) -> decltype(func()) {
        int attempt = 0;
        
        while (attempt <= max_retries) {
            try {
                return func();
            } catch (const std::exception& e) {
                attempt++;
                
                if (attempt > max_retries) {
                    ERROR("重试次数已用完，最后错误: {}", e.what());
                    throw;
                }
                
                auto delay = base_delay * (1 << (attempt - 1)); // 指数退避
                WARN("请求失败 (尝试 {}/{}): {}，{}ms后重试", 
                     attempt, max_retries, e.what(), delay.count());
                
                std::this_thread::sleep_for(delay);
            }
        }
        
        throw std::runtime_error("不应该到达这里");
    }
    
    std::string PostJsonWithRetry(const std::string& url, const std::string& json_data) {
        return executeWithRetry([&]() {
            return client.PostJson(url, json_data);
        });
    }
    
    std::string GetWithRetry(const std::string& url) {
        return executeWithRetry([&]() {
            return client.Get(url);
        });
    }
    
    std::string UploadFileWithRetry(const std::string& url, const std::string& file_path) {
        return executeWithRetry([&]() {
            return client.UploadFile(url, file_path);
        });
    }
};
```

### 连接池管理

```cpp
class HttpConnectionPool {
private:
    std::queue<std::unique_ptr<ExtendedHttpClient>> available_clients;
    std::mutex pool_mutex;
    size_t max_connections;
    size_t current_connections;
    
public:
    HttpConnectionPool(size_t max_conn = 10) 
        : max_connections(max_conn), current_connections(0) {}
    
    std::unique_ptr<ExtendedHttpClient> getClient() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (!available_clients.empty()) {
            auto client = std::move(available_clients.front());
            available_clients.pop();
            return client;
        }
        
        if (current_connections < max_connections) {
            current_connections++;
            return std::make_unique<ExtendedHttpClient>();
        }
        
        throw std::runtime_error("连接池已满");
    }
    
    void returnClient(std::unique_ptr<ExtendedHttpClient> client) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        available_clients.push(std::move(client));
    }
    
    ~HttpConnectionPool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        while (!available_clients.empty()) {
            available_clients.pop();
        }
    }
};

// RAII客户端管理
class PooledHttpClient {
private:
    HttpConnectionPool& pool;
    std::unique_ptr<ExtendedHttpClient> client;
    
public:
    PooledHttpClient(HttpConnectionPool& p) : pool(p) {
        client = pool.getClient();
    }
    
    ~PooledHttpClient() {
        if (client) {
            pool.returnClient(std::move(client));
        }
    }
    
    ExtendedHttpClient* operator->() {
        return client.get();
    }
    
    ExtendedHttpClient& operator*() {
        return *client;
    }
};
```

## 性能优化

### 1. 连接复用

```cpp
class PersistentHttpClient {
private:
    CURL* curl_;
    std::string base_url_;
    
public:
    PersistentHttpClient(const std::string& base_url) : base_url_(base_url) {
        curl_ = curl_easy_init();
        if (!curl_) {
            throw std::runtime_error("无法初始化CURL");
        }
        
        // 启用连接复用
        curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_, CURLOPT_TCP_KEEPIDLE, 120L);
        curl_easy_setopt(curl_, CURLOPT_TCP_KEEPINTVL, 60L);
    }
    
    ~PersistentHttpClient() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }
    
    std::string request(const std::string& endpoint, const std::string& method, 
                       const std::string& data = "") {
        std::string url = base_url_ + endpoint;
        std::string response;
        
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, method.c_str());
        
        if (!data.empty()) {
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
        }
        
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl_);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("请求失败: " + std::string(curl_easy_strerror(res)));
        }
        
        return response;
    }
};
```

### 2. 异步请求

```cpp
class AsyncHttpClient {
private:
    std::thread worker_thread;
    std::queue<std::function<void()>> task_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running{true};
    
public:
    AsyncHttpClient() {
        worker_thread = std::thread([this]() {
            workerLoop();
        });
    }
    
    ~AsyncHttpClient() {
        running = false;
        cv.notify_all();
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    std::future<std::string> postJsonAsync(const std::string& url, 
                                          const std::string& json_data) {
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push([=]() {
                try {
                    ExtendedHttpClient client;
                    std::string response = client.PostJson(url, json_data);
                    promise->set_value(response);
                } catch (const std::exception& e) {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        
        cv.notify_one();
        return future;
    }
    
private:
    void workerLoop() {
        while (running) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]() { return !task_queue.empty() || !running; });
            
            if (!running) break;
            
            if (!task_queue.empty()) {
                auto task = task_queue.front();
                task_queue.pop();
                lock.unlock();
                
                task();
            }
        }
    }
};
```

## 最佳实践

1. **使用HTTPS**：确保数据传输安全
2. **设置合适的超时**：避免长时间等待
3. **实现重试机制**：处理网络不稳定
4. **使用连接池**：提高性能
5. **错误处理**：妥善处理各种HTTP错误
6. **日志记录**：记录请求和响应信息
7. **认证管理**：安全地处理访问令牌
8. **数据验证**：验证请求和响应数据
9. **进度回调**：为大文件传输提供进度信息
10. **资源清理**：确保正确释放CURL资源