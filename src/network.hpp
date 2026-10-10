#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <fstream>
#include <deque>
#include "protocol.hpp"
#pragma comment(lib, "ws2_32.lib")

namespace ggsb {
class Connection {
    std::string host_, username_, password_;
    int port_;
    std::thread worker_;
    std::atomic<bool> stopping_{false}, online_{false}, auth_failed_{false};
    std::atomic<int> epoch_{0};
    mutable std::mutex mutex_;
    std::deque<std::string> outgoing_;
    std::vector<ggs::Message> incoming_;
    std::deque<std::string> who_queries_;
    std::string status_ = "未接続";
    std::ofstream log_;
    static uint64_t now() { return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
    void status(const std::string& value) { std::lock_guard lock(mutex_); status_ = value; }
    void log(const std::string& tag, const std::string& value) {
        if (log_) { log_ << now() << ' ' << tag << ' ' << value << '\n'; log_.flush(); }
    }
    void discard_pending() { std::lock_guard lock(mutex_); outgoing_.clear(); }
    static int receive(SOCKET socket, std::string& chunk) {
        fd_set read; FD_ZERO(&read); FD_SET(socket, &read);
        timeval timeout{0, 50000};
        int r = select(0, &read, nullptr, nullptr, &timeout);
        if (r <= 0) return r;
        char buffer[8192]; int n = recv(socket, buffer, sizeof(buffer), 0);
        if (n <= 0) return -1;
        chunk.assign(buffer, n); return 1;
    }
    bool write(SOCKET socket, const std::string& text, bool secret = false) {
        std::string bytes = text + "\n";
        size_t offset = 0; uint64_t deadline = now() + 5000;
        while (offset < bytes.size() && !stopping_ && now() < deadline) {
            int n = ::send(socket, bytes.data() + offset, int(bytes.size() - offset), 0);
            if (n > 0) offset += n;
            else if (n < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
                fd_set writable; FD_ZERO(&writable); FD_SET(socket, &writable); timeval timeout{0, 50000};
                if (select(0, nullptr, &writable, nullptr, &timeout) < 0) return false;
            } else return false;
        }
        if (offset != bytes.size()) return false;
        log("SEND", secret ? "[password redacted]" : text);
        if (!secret) {
            const auto words = ggs::tokenize(text);
            if (words.size() >= 4 && (words[0] == "t" || words[0] == "tell") && words[1] == "/os" && words[2] == "who") who_queries_.push_back(ggs::to_lower(words[3]));
        }
        return true;
    }
    // A timeout never means authenticated. Do not send any game commands before READY.
    bool await_prompt(SOCKET socket, ggs::MessageFramer& framer, const std::string& keyword, int ms) {
        uint64_t deadline = now() + ms;
        std::string seen;
        while (!stopping_ && now() < deadline) {
            std::string chunk; int r = receive(socket, chunk);
            if (r < 0) return false;
            if (!r) continue;
            framer.feed(chunk, now()); seen += ggs::to_lower(chunk);
            if (seen.size() > 65536) return false;
            if (keyword == "ready") {
                static const std::regex prompt(R"((^|[\r\n])\s*ready\s*($|[\r\n]))");
                if (std::regex_search(seen, prompt)) return true;
            } else if (seen.find(keyword) != std::string::npos) return true;
            if (seen.find("invalid password") != std::string::npos || seen.find("wrong password") != std::string::npos || seen.find("illegal login") != std::string::npos) return false;
        }
        return false;
    }
    SOCKET connect_socket() {
        addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; hints.ai_protocol = IPPROTO_TCP; hints.ai_family = AF_UNSPEC;
        addrinfo* addresses = nullptr;
        if (getaddrinfo(host_.c_str(), std::to_string(port_).c_str(), &hints, &addresses)) { status("ホスト名を解決できません"); return INVALID_SOCKET; }
        SOCKET socket_ = INVALID_SOCKET;
        for (auto* a = addresses; a && !stopping_; a = a->ai_next) {
            SOCKET s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
            if (s == INVALID_SOCKET) continue;
            u_long nonblocking = 1; ioctlsocket(s, FIONBIO, &nonblocking);
            bool ok = connect(s, a->ai_addr, int(a->ai_addrlen)) == 0;
            if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
                uint64_t deadline = now() + 8000;
                while (!stopping_ && now() < deadline) {
                    fd_set writable, errors; FD_ZERO(&writable); FD_ZERO(&errors); FD_SET(s, &writable); FD_SET(s, &errors);
                    timeval timeout{0, 100000};
                    int r = select(0, nullptr, &writable, &errors, &timeout);
                    if (r < 0 || FD_ISSET(s, &errors)) break;
                    if (r && FD_ISSET(s, &writable)) {
                        int error = 0, length = sizeof(error);
                        ok = getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0; break;
                    }
                }
            }
            if (ok) { socket_ = s; break; }
            closesocket(s);
        }
        freeaddrinfo(addresses); return socket_;
    }
    void deliver(ggs::MessageFramer& framer) {
        auto messages = framer.take();
        for (auto& m : messages) {
            const auto words = ggs::tokenize(m.head);
            if (words.size() >= 2 && words[0] == "/os:" && words[1] == "who" && !who_queries_.empty()) {
                m.rating_type = who_queries_.front(); who_queries_.pop_front();
            }
        }
        for (const auto& m : messages) log("RECV", m.text());
        std::lock_guard lock(mutex_);
        for (auto& m : messages) incoming_.push_back(std::move(m));
        if (incoming_.size() > 2000) incoming_.erase(incoming_.begin(), incoming_.begin() + (incoming_.size() - 2000));
    }
    void session(SOCKET socket) {
        ggs::MessageFramer framer;
        who_queries_.clear();
        status("ログインしています…");
        if (!await_prompt(socket, framer, "login", 10000) || !write(socket, username_)) { status("ログインプロンプトを受信できません"); return; }
        if (!await_prompt(socket, framer, "password", 10000) || !write(socket, password_, true) || !await_prompt(socket, framer, "ready", 10000)) {
            auth_failed_ = true; status("認証に失敗しました。接続設定を確認してください"); return;
        }
        framer.tick(now() + 1000); deliver(framer);
        for (const auto& command : {"ms /os", "ts client -", "ts vt100 -", "who", "chann + .chat .tourney", "t /os notify + *", "t /os notify +", "t /os match", "t /os who 8 a", "t /os who 8r a", "t /os request"}) {
            if (!write(socket, command)) return;
        }
        discard_pending(); ++epoch_; online_ = true; status("接続中 · " + username_);
        uint64_t keepalive = now();
        while (!stopping_) {
            std::deque<std::string> pending;
            { std::lock_guard lock(mutex_); pending.swap(outgoing_); }
            for (const auto& cmd : pending) if (!write(socket, cmd)) return;
            if (now() - keepalive > 60000) { if (!write(socket, "")) return; keepalive = now(); }
            std::string chunk; int r = receive(socket, chunk);
            if (r < 0) return;
            if (r > 0) framer.feed(chunk, now());
            framer.tick(now()); deliver(framer);
        }
    }
    void run() {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa)) { status("Winsockを初期化できません"); return; }
        int retry_seconds = 2;
        while (!stopping_) {
            status("接続しています · " + host_ + ":" + std::to_string(port_));
            SOCKET s = connect_socket();
            if (s != INVALID_SOCKET) {
                uint64_t began = now(); session(s); online_ = false; discard_pending();
                shutdown(s, SD_BOTH); closesocket(s);
                if (now() - began > 30000) retry_seconds = 2;
            }
            if (stopping_ || auth_failed_) break;
            status("切断 · " + std::to_string(retry_seconds) + "秒後に再接続");
            for (int i = 0; i < retry_seconds * 10 && !stopping_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            retry_seconds = std::min(retry_seconds * 2, 30);
        }
        online_ = false; WSACleanup();
    }
public:
    Connection(std::string host, int port, std::string username, std::string password, const std::string& log_path = {})
        : host_(std::move(host)), username_(std::move(username)), password_(std::move(password)), port_(port) { if (!log_path.empty()) log_.open(log_path); }
    ~Connection() { stop(); }
    Connection(const Connection&) = delete;
    void start() { if (!worker_.joinable()) worker_ = std::thread([this] { run(); }); }
    void stop() { stopping_ = true; if (worker_.joinable()) worker_.join(); }
    bool online() const { return online_; }
    int epoch() const { return epoch_; }
    std::string status() const { std::lock_guard lock(mutex_); return status_; }
    bool send(const std::string& command) {
        if (command.find_first_of("\r\n") != std::string::npos || command.size() > 4096) return false;
        std::lock_guard lock(mutex_);
        if (!online_ || outgoing_.size() >= 100) return false;
        outgoing_.push_back(command); return true;
    }
    std::vector<ggs::Message> poll() { std::lock_guard lock(mutex_); std::vector<ggs::Message> result; result.swap(incoming_); return result; }
};
}
