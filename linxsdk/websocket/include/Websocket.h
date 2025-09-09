#pragma once

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <libwebsockets.h>

#include "Log.h"

namespace linx {

class WebSocketClient {
public:
    WebSocketClient() = delete;

    WebSocketClient(const std::string& ws_url);
    ~WebSocketClient();

    void SetWsHeaders(const std::map<std::string, std::string>& ws_headers);
    void start();
    void send_text(const std::string& message);
    void send_binary(const void* data, size_t len);
    
    void SetOnOpenCallback(std::function<std::string(void)> cb);
    void SetOnCloseCallback(std::function<void(void)> cb);
    void SetOnFailCallback(std::function<void(void)> cb);
    void SetOnMessageCallback(std::function<std::string(const std::string&, bool)> cb);

private:
    static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                                  void *user, void *in, size_t len);
    
    void parse_url(const std::string& url);
    void run_event_loop();
    
    std::string ws_url_;
    std::string host_;
    std::string path_;
    int port_;
    bool use_ssl_;
    
    std::map<std::string, std::string> ws_headers_;
    
    struct lws_context *context_;
    struct lws *wsi_;
    struct lws_protocols protocols_[2];
    
    std::function<std::string(void)> on_open_cb_;
    std::function<std::string(const std::string&, bool)> on_message_cb_;
    std::function<void()> on_close_cb_;
    std::function<void()> on_fail_cb_;
    
    std::thread event_thread_;
    std::atomic<bool> running_;
    std::mutex send_mutex_;
    
    std::vector<std::string> send_queue_;
    std::mutex queue_mutex_;
};

}  // namespace linx