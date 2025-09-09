#include "Websocket.h"
#include <iostream>
#include <sstream>
#include <cstring>

namespace linx {

WebSocketClient::WebSocketClient(const std::string& ws_url) 
    : ws_url_(ws_url), context_(nullptr), wsi_(nullptr), running_(false) {
    
    parse_url(ws_url);
    
    // 初始化协议
    protocols_[0] = {
        "websocket-protocol",
        callback_websocket,
        0,
        1024,
        0, this, 0
    };
    protocols_[1] = { nullptr, nullptr, 0, 0, 0, nullptr, 0 };
}

WebSocketClient::~WebSocketClient() {
    running_ = false;
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
    if (context_) {
        lws_context_destroy(context_);
    }
}

void WebSocketClient::parse_url(const std::string& url) {
    // 简单的URL解析
    if (url.substr(0, 6) == "wss://") {
        use_ssl_ = true;
        std::string remaining = url.substr(6);
        size_t slash_pos = remaining.find('/');
        if (slash_pos != std::string::npos) {
            host_ = remaining.substr(0, slash_pos);
            path_ = remaining.substr(slash_pos);
        } else {
            host_ = remaining;
            path_ = "/";
        }
        port_ = 443;
    } else if (url.substr(0, 5) == "ws://") {
        use_ssl_ = false;
        std::string remaining = url.substr(5);
        size_t slash_pos = remaining.find('/');
        if (slash_pos != std::string::npos) {
            host_ = remaining.substr(0, slash_pos);
            path_ = remaining.substr(slash_pos);
        } else {
            host_ = remaining;
            path_ = "/";
        }
        port_ = 80;
    }
    
    // 检查端口号
    size_t colon_pos = host_.find(':');
    if (colon_pos != std::string::npos) {
        port_ = std::stoi(host_.substr(colon_pos + 1));
        host_ = host_.substr(0, colon_pos);
    }
}

void WebSocketClient::SetWsHeaders(const std::map<std::string, std::string>& ws_headers) {
    ws_headers_ = ws_headers;
}

void WebSocketClient::start() {
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols_;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    
    context_ = lws_create_context(&info);
    if (!context_) {
        ERROR("Failed to create libwebsockets context");
        return;
    }
    
    running_ = true;
    event_thread_ = std::thread(&WebSocketClient::run_event_loop, this);
}

void WebSocketClient::run_event_loop() {
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    
    ccinfo.context = context_;
    ccinfo.address = host_.c_str();
    ccinfo.port = port_;
    ccinfo.path = path_.c_str();
    ccinfo.host = host_.c_str();
    ccinfo.origin = host_.c_str();
    ccinfo.protocol = protocols_[0].name;
    ccinfo.userdata = this;
    
    if (use_ssl_) {
        ccinfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    }
    
    wsi_ = lws_client_connect_via_info(&ccinfo);
    if (!wsi_) {
        ERROR("Failed to connect to websocket server");
        return;
    }
    
    while (running_ && lws_service(context_, 50) >= 0) {
        // 处理发送队列
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!send_queue_.empty() && wsi_) {
            std::string msg = send_queue_.front();
            send_queue_.erase(send_queue_.begin());
            
            unsigned char buf[LWS_PRE + msg.length()];
            memcpy(&buf[LWS_PRE], msg.c_str(), msg.length());
            lws_write(wsi_, &buf[LWS_PRE], msg.length(), LWS_WRITE_TEXT);
        }
    }
}

void WebSocketClient::send_text(const std::string& message) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    send_queue_.push_back(message);
    INFO(">> {}", message);
}

void WebSocketClient::send_binary(const void* data, size_t len) {
    if (!wsi_) return;
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    unsigned char buf[LWS_PRE + len];
    memcpy(&buf[LWS_PRE], data, len);
    lws_write(wsi_, &buf[LWS_PRE], len, LWS_WRITE_BINARY);
}

void WebSocketClient::SetOnOpenCallback(std::function<std::string(void)> cb) {
    on_open_cb_ = cb;
}

void WebSocketClient::SetOnCloseCallback(std::function<void(void)> cb) {
    on_close_cb_ = cb;
}

void WebSocketClient::SetOnFailCallback(std::function<void(void)> cb) {
    on_fail_cb_ = cb;
}

void WebSocketClient::SetOnMessageCallback(std::function<std::string(const std::string&, bool)> cb) {
    on_message_cb_ = cb;
}

int WebSocketClient::callback_websocket(struct lws *wsi, enum lws_callback_reasons reason,
                                       void *user, void *in, size_t len) {
    WebSocketClient* client = static_cast<WebSocketClient*>(lws_context_user(lws_get_context(wsi)));
    if (!client && reason != LWS_CALLBACK_PROTOCOL_INIT) {
        client = static_cast<WebSocketClient*>(lws_wsi_user(wsi));
    }
    
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            INFO("WebSocket connection established");
            if (client && client->on_open_cb_) {
                std::string response = client->on_open_cb_();
                if (!response.empty()) {
                    client->send_text(response);
                }
            }
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (client && client->on_message_cb_) {
                std::string message(static_cast<char*>(in), len);
                bool is_binary = (lws_frame_is_binary(wsi) != 0);
                std::string response = client->on_message_cb_(message, is_binary);
                if (!response.empty()) {
                    client->send_text(response);
                }
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            ERROR("WebSocket connection error");
            if (client && client->on_fail_cb_) {
                client->on_fail_cb_();
            }
            break;
            
        case LWS_CALLBACK_CLOSED:
            INFO("WebSocket connection closed");
            if (client && client->on_close_cb_) {
                client->on_close_cb_();
            }
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // 可以发送数据
            break;
            
        default:
            break;
    }
    
    return 0;
}

}  // namespace linx