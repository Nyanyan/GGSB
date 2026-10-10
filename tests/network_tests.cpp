#include "../src/network.hpp"
#include "../src/demo.hpp"
#include <iostream>
#include <functional>
#include <stdexcept>
#include <filesystem>

static void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static void write_all(SOCKET socket, const std::string& data) {
    size_t offset = 0;
    while (offset < data.size()) { int n = send(socket, data.data() + offset, int(data.size() - offset), 0); check(n > 0, "mock send failed"); offset += n; }
}
static std::string read_line(SOCKET socket) {
    DWORD timeout = 4000; setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
    std::string line;
    for (int i = 0; i < 8192; ++i) { char c; int n = recv(socket, &c, 1, 0); check(n == 1, "mock receive failed"); if (c == '\n') return line; if (c != '\r') line += c; }
    throw std::runtime_error("mock line too long");
}
class MockServer {
    SOCKET listener = INVALID_SOCKET;
    std::thread worker;
public:
    int port = 0;
    std::string failure;
    explicit MockServer(std::function<void(SOCKET)> script, int sessions = 1) {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); check(listener != INVALID_SOCKET, "socket creation");
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        check(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "mock bind");
        int size = sizeof(address); getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size); port = ntohs(address.sin_port);
        check(listen(listener, 2) == 0, "mock listen");
        worker = std::thread([=, this] {
            try {
                for (int i = 0; i < sessions; ++i) {
                    fd_set readable; FD_ZERO(&readable); FD_SET(listener, &readable); timeval timeout{7, 0};
                    check(select(0, &readable, nullptr, nullptr, &timeout) > 0, "mock accept timeout");
                    SOCKET client = accept(listener, nullptr, nullptr); check(client != INVALID_SOCKET, "mock accept");
                    try { script(client); } catch (...) { closesocket(client); throw; }
                    shutdown(client, SD_BOTH); closesocket(client);
                }
            } catch (const std::exception& e) { failure = e.what(); }
        });
    }
    void join() { if (worker.joinable()) worker.join(); check(failure.empty(), failure.c_str()); }
    ~MockServer() { if (worker.joinable()) worker.join(); if (listener != INVALID_SOCKET) closesocket(listener); }
};
static bool eventually(std::function<bool()> predicate, int ms = 4500) {
    for (int i = 0; i < ms / 10; ++i) { if (predicate()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    return false;
}
static void authenticate(SOCKET socket, bool registered = true) {
    write_all(socket, "GGS\r\nEnter login (yours, or one you'd like to use).\r\n");
    check(read_line(socket) == "alice", "username");
    write_all(socket, "pass"); std::this_thread::sleep_for(std::chrono::milliseconds(20)); write_all(socket, "word: ");
    check(read_line(socket) == "secret-example", "password");
    write_all(socket, "\r\nREADY\r\n");
    bool chat_joined = false;
    for (int i = 0; i < 11; ++i) {
        const auto command = read_line(socket); check(!command.empty(), "initial command");
        if (command == "chann + .chat .tourney") chat_joined = true;
        if (command == "who") write_all(socket, std::string(": who 1 idle online ip-addr hostname\n|alice ") + (registered ? "+" : "-") + " 00:00 00:01 127.0.0.1 localhost <=\nREADY\n");
        if (command == "t /os who 8 a") write_all(socket, "/os: who 1 change: win draw loss matches\n|alice + 2200.5@100.0\nREADY\n");
        if (command == "t /os who 8r a") write_all(socket, "/os: who 1 change: win draw loss matches\n|alice + 2400.7@80.0\nREADY\n");
        check(command.find("rated") == std::string::npos && command.find(" open ") == std::string::npos, "no match settings before identity");
    }
    check(chat_joined, "public and tournament channels joined after login and reconnect");
}
int main() {
    WSADATA wsa{}; WSAStartup(MAKEWORD(2, 2), &wsa);
    try {
        std::atomic<int> rounds = 0;
        MockServer mock([&](SOCKET s) {
            authenticate(s);
            ggsb::ClientModel fixture; ggsb::Demo demo; demo.start(fixture, 0);
            std::string raw = ggsb::board_message(fixture.matches.at(".117").game[0], "s8r14", true).text() + "\nREADY\r\n";
            for (size_t offset = 0; offset < raw.size(); offset += 17) write_all(s, raw.substr(offset, 17));
            check(read_line(s) == "t /os match", "command after login");
            check(read_line(s) == "tell bob private test", "private chat sent to player");
            write_all(s, "bob: private reply\nREADY\n");
            check(read_line(s) == "tell .chat public test", "global chat sent to channel");
            write_all(s, ".chat carol: public reply\nREADY\n");
            ++rounds;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }, 2);
        ggsb::Connection connection("127.0.0.1", mock.port, "alice", "secret-example", "build/network-test.log");
        check(!connection.send("t /os play .1 D3"), "offline commands rejected"); connection.start();
        if (!eventually([&] { return connection.online(); })) {
            connection.stop(); mock.join();
            throw std::runtime_error("login completion: " + connection.status());
        }
        check(!connection.send("t /os match\nquit"), "newline command rejected");
        check(connection.send("t /os match"), "online command accepted");
        check(connection.send("tell bob private test") && connection.send("tell .chat public test"), "chat commands accepted");
        std::vector<ggs::Message> messages;
        check(eventually([&] { for (auto& m : connection.poll()) messages.push_back(std::move(m)); return rounds >= 1; }), "first session completed");
        check(eventually([&] { return !connection.online(); }), "disconnect detected");
        check(!connection.send("t /os resign .1"), "destructive offline command discarded");
        check(eventually([&] { return connection.epoch() == 2 && connection.online(); }, 5000), "automatic reconnect");
        check(connection.send("t /os match"), "command on new connection");
        check(connection.send("tell bob private test") && connection.send("tell .chat public test"), "chat commands after reconnect");
        check(eventually([&] { for (auto& m : connection.poll()) messages.push_back(std::move(m)); return rounds == 2; }), "second session completed");
        connection.stop(); mock.join();
        int boards = 0;
        for (const auto& message : messages) { ggs::BoardMessage b; if (ggs::parse_board_message(message, b)) ++boards; }
        check(boards >= 1, "fragmented board parsed");
        ggsb::ClientModel registered_model; registered_model.username = "alice";
        for (const auto& message : messages) registered_model.receive(message, 0);
        check(registered_model.registered == true, "registered account recognized");
        auto* alice = registered_model.find_user("alice");
        check(alice && alice->rating == 2200.5 && alice->random_rating == 2400.7, "untagged who replies associated with both rating types across reconnect");
        check(registered_model.chats.threads.at("bob").messages.back().text == "private reply", "received private chat separated by player");
        check(registered_model.chats.threads.at(".chat").messages.back().text == "public reply", "received global chat separated from private chat");
        std::ifstream file("build/network-test.log"); std::string logged((std::istreambuf_iterator<char>(file)), {});
        check(logged.find("secret-example") == std::string::npos, "password absent from logs");
        check(logged.find("[password redacted]") != std::string::npos, "password redaction marker");
        std::atomic<bool> guest_lobby_received = false;
        MockServer guest([&](SOCKET s) {
            authenticate(s, false);
            write_all(s, "/os: ERR you are not registered.\nREADY\n");
            check(read_line(s) == "t /os match", "guest can observe lobby");
            guest_lobby_received = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        });
        ggsb::Connection guest_connection("127.0.0.1", guest.port, "alice", "secret-example"); guest_connection.start();
        check(eventually([&] { return guest_connection.online(); }), "unregistered login succeeds");
        check(guest_connection.send("t /os match"), "guest lobby command");
        ggsb::ClientModel guest_model; guest_model.username = "alice";
        check(eventually([&] { for (const auto& message : guest_connection.poll()) guest_model.receive(message, 0); return guest_model.registered == false; }), "unregistered account recognized");
        check(eventually([&] { return guest_lobby_received.load(); }), "guest lobby command reaches server");
        guest_connection.stop(); guest.join();
        for (const auto& message : guest_model.console) check(!ggsb::unregistered_notice(message), "registration notice omitted from console");
        MockServer invalid([](SOCKET s) {
            write_all(s, "Enter login\n"); check(read_line(s) == "alice", "invalid username");
            write_all(s, "password:\n"); check(read_line(s) == "secret-example", "invalid password");
            write_all(s, "Wrong password. You are already registered.\nEnter login\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        });
        ggsb::Connection rejected("127.0.0.1", invalid.port, "alice", "secret-example"); rejected.start();
        check(eventually([&] { return rejected.status().find("認証に失敗") != std::string::npos; }), "authentication failure reported");
        check(!rejected.online() && rejected.epoch() == 0, "never online with wrong password"); rejected.stop(); invalid.join();
        MockServer hanging([](SOCKET s) { write_all(s, "Enter login\n"); read_line(s); std::this_thread::sleep_for(std::chrono::milliseconds(500)); });
        ggsb::Connection cancelled("127.0.0.1", hanging.port, "alice", "secret-example"); cancelled.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto before = std::chrono::steady_clock::now(); cancelled.stop();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
        check(duration < 1000, "shutdown interrupts authentication wait"); hanging.join();
        WSACleanup(); std::cout << "PASS: login, registered/guest identity, quiet guest login, fragmented messages, commands, disconnect, reconnect, auth rejection, password redaction, cancellation\n";
        return 0;
    } catch (const std::exception& e) { WSACleanup(); std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
