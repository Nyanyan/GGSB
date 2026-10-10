#pragma once
#include <array>
#include <map>
#include <set>
#include <deque>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <optional>
#include "othello.hpp"

namespace ggsb {
inline bool server_error(const ggs::Message& message) {
    const auto words = ggs::tokenize(message.head);
    if (words.size() < 2 || (words[0] != ":" && !(words[0].starts_with("/") && words[0].ends_with(":")))) return false;
    const auto kind = ggs::to_lower(words[1]);
    return kind == "err" || kind == "error";
}
inline bool unregistered_notice(const ggs::Message& message) {
    const auto head = ggs::to_lower(message.head);
    return (head.starts_with("/os: err ") || head.starts_with("/os: error ")) && ggs::to_lower(message.text()).find("you are not registered.") != std::string::npos;
}
struct EvalPoint { int ply; double value; bool exact = false; };
using EvalSeries = std::vector<std::pair<int, double>>;
inline double graph_range(const EvalSeries& a, const EvalSeries& b) {
    double range = 4;
    for (const auto* values : {&a, &b}) for (const auto& point : *values) range = std::max(range, std::abs(point.second));
    return std::ceil(range / 2) * 2;
}
struct Game {
    std::string id;
    std::string name[2];
    double rating[2]{};
    Position pos{}, previous{};
    bool active = false, finished = false, history_ok = false;
    bool has_official_result = false;
    double result_black = 0;
    int ply = 0, last_cell = -1;
    int clock_s[2]{};
    bool has_clock = false;
    uint64_t clock_ms = 0, changed_ms = 0;
    std::vector<Position> history;
    std::vector<ggs::MoveInfo> moves;
    std::vector<EvalPoint> evals[2];
    bool sends_eval[2]{};
    int starting_discs = 4;
    std::map<int, int> received_placements;
    int initial_discs() const { return history.empty() ? starting_discs : std::popcount(history.front().black | history.front().white); }
    int move_limit() const { return 64 - initial_discs(); }
    int placed() const { return std::clamp(std::popcount(pos.black | pos.white) - initial_discs(), 0, move_limit()); }
    // Protocol plies include passes; graph coordinates count only newly occupied squares.
    std::optional<int> placed_at(int at_ply) const {
        if (at_ply == 0) return 0;
        if (at_ply >= 0 && at_ply < int(history.size())) return std::popcount(history[at_ply].black | history[at_ply].white) - initial_discs();
        if (auto it = received_placements.find(at_ply); it != received_placements.end()) return it->second;
        if (at_ply == ply && active) return placed();
        return {};
    }
    int ply_at_placement(int count) const {
        int result = 0;
        for (int i = 0; i < int(history.size()); ++i) {
            if (std::popcount(history[i].black | history[i].white) - initial_discs() > count) break;
            result = i; // Include a pass following this placement without advancing x.
        }
        return result;
    }
    EvalSeries graph_series(int color) const {
        EvalSeries result;
        if (!active || color < 0 || color > 1) return result;
        for (const auto& point : evals[color]) if (auto x = placed_at(point.ply)) {
            if (!result.empty() && result.back().first == *x) result.back().second = point.value;
            else result.emplace_back(*x, point.value);
        }
        return result;
    }
    int color_of(const std::string& who) const {
        for (int c = 0; c < 2; ++c) if (!who.empty() && name[c] == who) return c;
        return -1;
    }
    int remaining(int c, uint64_t now, bool online) const {
        return clock_s[c] - (online && !finished && pos.to_move == c && now >= clock_ms ? int((now - clock_ms) / 1000) : 0);
    }
    double final_score() const { return has_official_result ? result_black : score_black(pos); }
    // -1 denotes a draw; an absent result includes an aborted or unseen game.
    std::optional<int> winner() const {
        if (!active || !finished || (!has_official_result && !is_game_over(pos))) return {};
        const double score = final_score();
        return score > 0.00001 ? BLACK : score < -0.00001 ? WHITE : -1;
    }
    std::optional<double> evaluation(int c, int at_ply) const {
        for (auto it = evals[c].rbegin(); it != evals[c].rend(); ++it) if (it->ply <= at_ply) return it->value;
        return {};
    }
    void record_eval(int mover, const ggs::MoveInfo& move, const Position* before) {
        if (mover < 0 || mover > 1) return;
        if (move.has_eval) sends_eval[mover] = true;
        if (move.pass || (before && std::popcount(legal_moves(*before)) <= 1)) return;
        if (!move.has_eval && (!before || !sends_eval[mover])) return;
        evals[mover].push_back({move.ply, move.has_eval ? move.eval : 0.0});
    }
    void final_evals() {
        if (!winner()) return;
        for (int c = 0; c < 2; ++c) {
            const double v = c == BLACK ? final_score() : -final_score();
            if (!evals[c].empty() && evals[c].back().exact) evals[c].back().value = v;
            else evals[c].push_back({ply, v, true});
        }
    }
    void receive(const ggs::BoardMessage& bm, uint64_t now) {
        const auto server = bm.boards.back();
        if (!active && !bm.is_join) {
            const auto random = bm.type.find_first_of("rR");
            int declared = 0;
            if (random != std::string::npos && ggs::parse_int(bm.type.substr(random + 1), declared) && declared >= 4 && declared <= 64) starting_discs = declared;
        }
        previous = pos; changed_ms = now;
        for (int c = 0; c < 2; ++c) if (bm.clock[c].valid) {
            name[c] = bm.clock[c].name; rating[c] = bm.clock[c].rating;
            clock_s[c] = bm.clock[c].seconds; has_clock = true;
        }
        clock_ms = now;
        if (bm.is_join) {
            history.clear(); moves.clear(); evals[0].clear(); evals[1].clear();
            received_placements.clear();
            starting_discs = 4;
            const auto random = bm.type.find_first_of("rR");
            int declared = 0;
            if (random != std::string::npos && ggs::parse_int(bm.type.substr(random + 1), declared) && declared >= 4 && declared <= 64) starting_discs = declared;
            if (bm.boards.size() >= 2 || bm.move_count == 0) starting_discs = std::popcount(bm.boards.front().black | bm.boards.front().white);
            sends_eval[0] = sends_eval[1] = false;
            finished = has_official_result = false;
            history_ok = bm.boards.size() >= 2 || (bm.moves.empty() && bm.move_count <= 0);
            Position p = bm.boards.front();
            if (history_ok) history.push_back(p);
            for (const auto& mv : bm.moves) {
                if (!history_ok) break;
                if (mv.ply != int(moves.size()) + 1) { history_ok = false; break; }
                record_eval(p.to_move, mv, &p);
                if (!apply_move(p, mv.pass, mv.cell)) { history_ok = false; break; }
                moves.push_back(mv); history.push_back(p);
            }
            ply = std::max(bm.move_count, bm.moves.empty() ? 0 : bm.moves.back().ply);
            history_ok = history_ok && p == server && int(history.size()) == ply + 1;
            last_cell = bm.moves.empty() || bm.moves.back().pass ? -1 : bm.moves.back().cell;
        } else if (!bm.moves.empty()) {
            const auto& mv = bm.moves.back();
            if (active && mv.ply == ply + 1) {
                Position p = pos;
                record_eval(p.to_move, mv, &p);
                if (!apply_move(p, mv.pass, mv.cell) || !(p == server)) history_ok = false;
                else if (history_ok) { moves.push_back(mv); history.push_back(p); }
            } else if (!active || mv.ply != ply || !(server == pos)) {
                history_ok = false;
                record_eval(server.to_move ^ 1, mv, nullptr);
                if (mv.ply < ply) { evals[0].clear(); evals[1].clear(); finished = has_official_result = false; }
            }
            ply = mv.ply; last_cell = mv.pass ? -1 : mv.cell;
        } else if (!(pos == server)) {
            history_ok = false;
            if (bm.move_count >= 0) ply = bm.move_count;
        }
        pos = server; active = true;
        received_placements[ply] = placed();
        if (is_game_over(pos)) finished = true;
        final_evals();
    }
};

struct Match {
    std::string id, type = "8", flag = "U";
    std::string player[2];
    Game game[2];
    bool watching = false, finished = false, listed = true;
    bool has_result = false;
    bool closed_event = false;
    double result_p0 = 0;
    uint64_t watch_ms = 0;
    int games() const { return !type.empty() && (type[0] == 's' || type[0] == 'S') ? 2 : 1; }
    bool clock_running(int sub) const {
        if (!game[sub].active || game[sub].finished) return false;
        return games() == 1 || !game[1 - sub].active || game[1 - sub].finished || game[sub].ply <= game[1 - sub].ply;
    }
    bool complete() const {
        for (int i = 0; i < games(); ++i) if (!game[i].active || !game[i].finished) return false;
        return true;
    }
    int max_ply() const { return std::max(game[0].ply, games() == 2 ? game[1].ply : 0); }
    int x_max(int sub = -1) const {
        auto board_limit = [&](int i) {
            if (game[i].active || !game[i].history.empty()) return game[i].move_limit();
            int declared = 4;
            const auto random = type.find_first_of("rR");
            if (random != std::string::npos) ggs::parse_int(type.substr(random + 1), declared);
            return 64 - std::clamp(declared, 0, 64);
        };
        if (sub >= 0 && sub < games()) return board_limit(sub);
        int limit = 0;
        for (int i = 0; i < games(); ++i) limit = std::max(limit, board_limit(i));
        return limit;
    }
    int max_placements(int sub = -1) const {
        if (sub >= 0 && sub < games()) return game[sub].placed();
        return std::max(game[0].placed(), games() == 2 ? game[1].placed() : 0);
    }
    int empty_squares(int count, int sub = -1) const { return std::clamp(x_max(sub) - count, 0, x_max(sub)); }
    int graph_divergence() const {
        if (games() != 2 || !game[0].history_ok || !game[1].history_ok) return -1;
        for (int x = 0; x <= std::min(game[0].placed(), game[1].placed()); ++x)
            if (!(game[0].history[game[0].ply_at_placement(x)] == game[1].history[game[1].ply_at_placement(x)])) return x;
        return -1;
    }
    int divergence() const {
        if (games() != 2 || !game[0].history_ok || !game[1].history_ok) return -1;
        const size_t n = std::min(game[0].history.size(), game[1].history.size());
        for (size_t i = 0; i < n; ++i) if (!(game[0].history[i] == game[1].history[i])) return int(i);
        return -1;
    }
    int total_discs(int idx, int at_ply = -1) const {
        int n = 0;
        for (int i = 0; i < games(); ++i) {
            int c = game[i].color_of(player[idx]);
            Position pos = game[i].pos;
            if (at_ply >= 0 && game[i].history_ok && !game[i].history.empty()) pos = game[i].history[std::min(at_ply, int(game[i].history.size()) - 1)];
            if (game[i].active && c >= 0) n += disc_count(pos, c);
        }
        return n;
    }
    // Each line is one player's own estimates, summed across the two boards.
    // The UI negates player 1 so both lines share player 0's perspective.
    std::vector<std::pair<int, double>> series(int idx) const {
        std::vector<int> plies;
        int colors[2]{};
        for (int i = 0; i < games(); ++i) {
            colors[i] = game[i].color_of(player[idx]);
            if (!game[i].active || colors[i] < 0) return {};
            for (const auto& point : game[i].evals[colors[i]]) plies.push_back(point.ply);
        }
        std::sort(plies.begin(), plies.end());
        plies.erase(std::unique(plies.begin(), plies.end()), plies.end());
        std::vector<std::pair<int, double>> result;
        for (int ply : plies) {
            double sum = 0; bool known = true;
            for (int i = 0; i < games(); ++i) {
                auto v = game[i].evaluation(colors[i], ply);
                if (!v) { known = false; break; }
                sum += *v;
            }
            if (known) result.emplace_back(ply, sum);
        }
        if (has_result && finished && complete()) {
            const double official = result_p0 * games() * (idx == 0 ? 1 : -1);
            if (!result.empty() && result.back().first == max_ply()) result.back().second = official;
            else result.emplace_back(max_ply(), official);
        }
        return result;
    }
    std::vector<std::pair<int, double>> board_series(int sub, int idx) const {
        std::vector<std::pair<int, double>> result;
        if (sub < 0 || sub >= games() || idx < 0 || idx > 1) return result;
        const auto& g = game[sub]; const int color = g.color_of(player[idx]);
        if (!g.active || color < 0) return result;
        for (const auto& point : g.evals[color]) result.emplace_back(point.ply, point.value);
        return result;
    }
    EvalSeries graph_series(int idx, int sub = -1) const {
        if (idx < 0 || idx > 1 || sub >= games()) return {};
        if (sub >= 0) return game[sub].graph_series(game[sub].color_of(player[idx]));
        std::array<EvalSeries, 2> boards;
        std::set<int> coordinates;
        int available = max_placements();
        for (int i = 0; i < games(); ++i) {
            boards[i] = game[i].graph_series(game[i].color_of(player[idx]));
            if (boards[i].empty()) return {};
            if (!game[i].finished) available = std::min(available, game[i].placed());
            for (const auto& point : boards[i]) coordinates.insert(point.first);
        }
        EvalSeries result;
        for (int x : coordinates) {
            if (x > available) break;
            double sum = 0; bool known = true;
            for (int i = 0; i < games(); ++i) {
                auto it = std::find_if(boards[i].rbegin(), boards[i].rend(), [x](const auto& point) { return point.first <= x; });
                if (it == boards[i].rend()) { known = false; break; }
                sum += it->second;
            }
            if (known) result.emplace_back(x, sum);
        }
        if (has_result && finished && complete()) {
            const double official = result_p0 * games() * (idx == 0 ? 1 : -1);
            if (!result.empty() && result.back().first == max_placements()) result.back().second = official;
            else result.emplace_back(max_placements(), official);
        }
        return result;
    }
    double graph_range_for(int sub = -1) const {
        if (games() == 2 && sub >= 0)
            return std::max(graph_range(graph_series(0, 0), graph_series(1, 0)), graph_range(graph_series(0, 1), graph_series(1, 1)));
        return graph_range(graph_series(0, sub), graph_series(1, sub));
    }
    std::optional<double> result_sum() const {
        if (has_result) return result_p0 * games();
        if (!complete()) return {};
        double sum = 0;
        for (int i = 0; i < games(); ++i) {
            int c = game[i].color_of(player[0]);
            if (c < 0 || !game[i].winner()) return {};
            sum += game[i].final_score() * (c == BLACK ? 1 : -1);
        }
        return sum;
    }
    std::optional<int> winner() const {
        if (!finished && !complete()) return {};
        if (auto score = result_sum()) return *score > 0.00001 ? 0 : *score < -0.00001 ? 1 : -1;
        return {};
    }
};

struct User {
    std::string name, state;
    std::optional<double> rating, random_rating;
};
struct Request {
    std::string id, from, to, type, clock, flag;
    bool removed = false;
};
inline bool supported_type(const std::string& type);
inline bool parse_request_line(const std::string& line, Request& out) {
    auto w = ggs::tokenize(line);
    size_t i = 0;
    while (i < w.size() && !ggs::is_match_id_token(w[i])) ++i;
    if (w.size() < i + 8) return false;
    double rating;
    if (!ggs::parse_double(w[i + 1], rating)) return false;
    out.id = w[i]; out.from = w[i + 2]; out.clock = w[i + 3]; out.type = w[i + 4];
    out.flag = w[i + 5]; out.to = w[i + 7];
    if (!supported_type(out.type)) return false;
    return true;
}
inline bool token_safe(const std::string& s, bool allow_empty = false) {
    return (allow_empty || !s.empty()) && s.size() <= 128 && s.find_first_of("\r\n\t ") == std::string::npos;
}
inline std::optional<std::string> player_info_command(const std::string& name, bool game_stats = false) {
    if (!token_safe(name)) return {};
    return (game_stats ? "t /os finger " : "finger ") + name;
}
inline bool supported_type(const std::string& type) {
    static const std::regex re(R"((?:s8(?:r\d+)?|8[bw]?))", std::regex::icase);
    return std::regex_match(type, re);
}
inline bool valid_clock(const std::string& clock) {
    if (!token_safe(clock)) return false;
    static const std::regex component(R"(([^,]+)(?:,([Nn]?\d*))?)");
    size_t start = 0; int parts = 0;
    while (true) {
        const auto slash = clock.find('/', start);
        const auto part = clock.substr(start, slash == std::string::npos ? slash : slash - start);
        if (++parts > 3 || (parts == 1 && part.empty())) return false;
        if (!part.empty()) {
            std::smatch match; int seconds = 0;
            if (!std::regex_match(part, match, component) || !ggs::parse_hhmmss(match[1].str(), seconds) || seconds < 0) return false;
            std::string moves = match[2].str();
            if (!moves.empty() && (moves[0] == 'N' || moves[0] == 'n')) moves.erase(0, 1);
            int count = 0; if (!moves.empty() && (!ggs::parse_int(moves, count) || count < 0)) return false;
        }
        if (slash == std::string::npos) return true;
        start = slash + 1;
    }
}
inline std::optional<std::string> chat_command(const std::string& target, const std::string& value, const std::string& match_id) {
    if (value.empty() || value.size() > 2048 || !token_safe(target) || value.find_first_of("\r\n") != std::string::npos) return {};
    if (target == "/os" || target == "kibitz" || target == "whisper") {
        if (!ggs::is_match_id_token(match_id)) return {};
        return "t /os tell " + std::string(target == "whisper" ? "o " : "a ") + match_id + " " + value;
    }
    return "tell " + target + " " + value;
}

inline bool conversation_target(const std::string& target) {
    if (!token_safe(target) || target == "/" || target.find(',') != std::string::npos || target[0] == '_' || ggs::is_match_id_token(target)) return false;
    return std::all_of(target.begin(), target.end(), [](unsigned char ch) { return ch > 32 && ch != 127; });
}
inline std::optional<std::string> conversation_command(const std::string& target, const std::string& value) {
    if (!conversation_target(target) || value.empty() || value.size() > 2048 || value.find_first_of("\r\n") != std::string::npos) return {};
    return "tell " + target + " " + value;
}
struct ChatEntry { std::string sender, text; bool outgoing = false; };
struct ChatThread {
    std::deque<ChatEntry> messages;
    uint64_t first_message = 0;
    int unread = 0;
};
class ChatBook {
public:
    std::map<std::string, ChatThread> threads{{".chat", {}}, {".tourney", {}}};
    std::vector<std::string> order{".chat", ".tourney"};
    std::string selected = ".chat";
    bool is_open(const std::string& target) const { return std::find(order.begin(), order.end(), target) != order.end(); }
    ChatThread& ensure(const std::string& target) {
        auto it = threads.try_emplace(target).first;
        if (!is_open(target)) order.push_back(target);
        return it->second;
    }
    bool open(const std::string& target) {
        if (!conversation_target(target)) return false;
        ensure(target).unread = 0; selected = target; return true;
    }
    bool close(const std::string& target) {
        if (target == ".chat" || target == ".tourney" || !is_open(target)) return false;
        std::erase(order, target);
        threads.at(target).unread = 0;
        if (selected == target) selected = ".chat";
        return true;
    }
    void append(const std::string& target, const std::string& sender, const std::string& text, bool outgoing = false) {
        auto& thread = ensure(target);
        thread.messages.push_back({sender, text, outgoing});
        if (!outgoing) ++thread.unread;
        while (thread.messages.size() > 500) { thread.messages.pop_front(); ++thread.first_message; }
    }
    void receive_service(const ggs::Message& message) {
        if (!message.head.starts_with("/")) return;
        const auto colon = message.head.find(':');
        if (colon == std::string::npos) return;
        const auto service = message.head.substr(0, colon);
        // Copy replies into an explicitly opened service conversation. They must
        // still reach the match, board, rating and request parsers below.
        if (!conversation_target(service) || !is_open(service)) return;
        std::string text = ggs::trim(message.head.substr(colon + 1));
        if (text.empty() && message.body.empty()) return;
        for (const auto& line : message.body) text += "\n" + line;
        append(service, service, text);
    }
    bool receive(const ggs::Message& message) {
        // Central GGS: "name: text" or ".channel name: text". Service replies
        // are copied separately without consuming their protocol updates.
        static const std::regex private_line(R"(^([^,\s]+):[ \t]+(.*)$)");
        static const std::regex channel_line(R"(^([.][^,\s]+)\s+([^,\s]+):[ \t]+(.*)$)");
        std::smatch match; std::string target, sender, text;
        if (std::regex_match(message.head, match, channel_line)) { target = match[1]; sender = match[2]; text = match[3]; }
        else if (std::regex_match(message.head, match, private_line)) { target = sender = match[1]; text = match[2]; }
        else return false;
        if (!conversation_target(target) || target.starts_with("/")) return false;
        // The authentication prompt "password: " has no message body.
        if (ggs::trim(text).empty() && message.body.empty()) return false;
        for (const auto& line : message.body) text += "\n" + line;
        append(target, sender, text); return true;
    }
};

class ClientModel {
public:
    std::map<std::string, Match> matches;
    std::vector<User> users;
    std::array<std::map<std::string, User>, 2> who_snapshots;
    std::array<uint64_t, 2> who_received{};
    uint64_t who_sequence = 0;
    std::map<std::string, Request> requests;
    std::set<std::string> watches;
    std::set<std::string> stopped_watches, moved_matches;
    std::deque<ggs::Message> console;
    ChatBook chats;
    std::map<std::string, std::deque<std::string>> match_chat;
    std::string username, selected;
    std::optional<bool> registered;
    uint64_t console_first_line = 0;
    uint64_t move_events = 0;
    const User* find_user(const std::string& name) const {
        auto it = std::find_if(users.begin(), users.end(), [&](const auto& u) { return u.name == name; });
        return it == users.end() ? nullptr : &*it;
    }
    Match& ensure(const std::string& id) {
        auto& m = matches[id]; m.id = id; return m;
    }
    void begin_watching(const std::string& id, uint64_t now) {
        stopped_watches.erase(id); watches.insert(id);
        auto& m = ensure(id); m.watching = true; m.watch_ms = now;
    }
    bool can_stop_watching(const std::string& id) const {
        const auto found = matches.find(id);
        if (found == matches.end() || (!found->second.watching && !watches.count(id))) return false;
        const auto& m = found->second;
        if (!m.finished && !username.empty()) {
            if (m.player[0] == username || m.player[1] == username) return false;
            for (int sub = 0; sub < m.games(); ++sub) if (m.game[sub].color_of(username) >= 0) return false;
        }
        return true;
    }
    bool stop_watching(const std::string& id) {
        if (!can_stop_watching(id)) return false;
        watches.erase(id); stopped_watches.insert(id); moved_matches.erase(id);
        matches.at(id).watching = false;
        if (selected == id) selected.clear();
        return true;
    }
    void append_console(const ggs::Message& message) {
        console.push_back(message);
        if (console.size() > 500) { console_first_line += 1 + console.front().body.size(); console.pop_front(); }
    }
    void clear_console() { for (const auto& message : console) console_first_line += 1 + message.body.size(); console.clear(); }
    void receive(const ggs::Message& message, uint64_t now) {
        if (unregistered_notice(message)) { registered = false; return; }
        const auto central_header = ggs::tokenize(message.head);
        if (central_header.size() >= 2 && central_header[0] == ":" && central_header[1] == "who") for (const auto& row : message.body) {
            const auto fields = ggs::tokenize(row);
            if (fields.size() >= 2 && (fields[0] == username || fields.back() == "<=") && (fields[1] == "+" || fields[1] == "-")) registered = fields[1] == "+";
        }
        append_console(message);
        chats.receive_service(message);
        if (chats.receive(message)) return;
        std::vector<ggs::MatchRow> rows;
        if (ggs::parse_match_list(message, rows)) {
            for (auto& [id, m] : matches) m.listed = false;
            for (const auto& row : rows) if (supported_type(row.type)) update_row(row, true);
            return;
        }
        ggs::MatchEvent event;
        if (ggs::parse_match_event(message, event)) {
            if (!supported_type(event.row.type)) return;
            auto& m = update_row(event.row, event.start);
            if (!event.start) {
                m.listed = false; m.finished = true;
                m.closed_event = true;
                m.has_result = event.has_result; m.result_p0 = event.result0;
                if (m.games() == 1 && m.has_result) {
                    auto& g = m.game[0]; int c = g.color_of(m.player[0]);
                    if (c >= 0) { g.has_official_result = true; g.result_black = m.result_p0 * (c == BLACK ? 1 : -1); }
                }
                for (int i = 0; i < m.games(); ++i) { m.game[i].finished = true; m.game[i].final_evals(); }
            }
            return;
        }
        ggs::BoardMessage bm;
        if (ggs::parse_board_message(message, bm)) {
            if (!bm.type.empty() && !supported_type(bm.type)) return;
            if (bm.sub > 1 || bm.sub < -1) return;
            auto& m = ensure(bm.match_id);
            if (!bm.type.empty()) m.type = bm.type;
            const int sub = std::max(0, bm.sub);
            if (bm.sub >= 0 && m.games() == 1) m.type = "s8";
            auto& g = m.game[sub];
            const bool first_board = !g.active;
            const int previous_ply = g.ply;
            const int previous_discs = std::popcount(g.pos.black | g.pos.white);
            g.id = bm.game_id; g.receive(bm, now);
            if (m.games() == 2 && g.ply != previous_ply && g.ply == m.game[1 - sub].ply) m.game[1 - sub].clock_ms = now;
            if (m.player[0].empty()) {
                m.player[0] = g.name[sub == 1 ? WHITE : BLACK];
                m.player[1] = g.name[sub == 1 ? BLACK : WHITE];
            }
            const bool own = !m.finished && g.color_of(username) >= 0;
            if (own) stopped_watches.erase(m.id);
            m.watching = !stopped_watches.count(m.id);
            if (m.watching) watches.insert(m.id);
            m.finished = m.complete();
            if (m.watching && !first_board && !bm.is_join && g.ply > previous_ply
                && std::popcount(g.pos.black | g.pos.white) > previous_discs) { ++move_events; moved_matches.insert(m.id); }
            if (m.watching && (selected.empty() || (first_board && own))) selected = m.id;
            return;
        }
        ggs::GameEnd end;
        if (ggs::parse_game_end(message, end)) {
            auto found = matches.find(end.match_id);
            if (found == matches.end()) return;
            auto& m = found->second; auto& g = m.game[std::clamp(end.sub, 0, 1)];
            g.finished = true;
            int c = g.color_of(end.player[0]);
            if (end.has_result && c >= 0) { g.has_official_result = true; g.result_black = end.result0 * (c == BLACK ? 1 : -1); }
            g.final_evals(); m.finished = m.complete(); return;
        }
        auto head = ggs::tokenize(message.head);
        if (head.size() >= 2 && head[0] == "/os:" && head[1] == "who") {
            if (message.rating_type != "8" && message.rating_type != "8r") return;
            const int kind = message.rating_type == "8r" ? 1 : 0;
            auto& snapshot = who_snapshots[kind]; snapshot.clear(); who_received[kind] = ++who_sequence;
            static const std::regex re(R"(^\s*(\S+)\s+([+\-x])\s+([-+0-9.]+)@)");
            for (const auto& line : message.body) {
                std::smatch r;
                double rating = 0;
                if (std::regex_search(line, r, re) && ggs::parse_double(r[3], rating) && token_safe(r[1])) {
                    User u{r[1], r[2], {}, {}};
                    (kind == 0 ? u.rating : u.random_rating) = rating;
                    snapshot.emplace(u.name, std::move(u));
                }
            }
            std::map<std::string, User> combined;
            const int first = who_received[0] <= who_received[1] ? 0 : 1;
            for (int idx : {first, 1 - first}) for (const auto& [name, row] : who_snapshots[idx]) {
                auto& u = combined[name]; u.name = name; u.state = row.state;
                if (idx == 0) u.rating = row.rating; else u.random_rating = row.random_rating;
            }
            users.clear(); for (auto& [name, row] : combined) users.push_back(std::move(row));
            std::sort(users.begin(), users.end(), [](const User& a, const User& b) { return a.rating != b.rating ? a.rating > b.rating : a.name < b.name; });
            return;
        }
        if (head.size() >= 2 && head[0] == "/os:" && head[1] == "request") {
            requests.clear();
            for (const auto& line : message.body) { Request r; if (parse_request_line(line, r)) requests[r.id] = r; }
            return;
        }
        if (head.size() >= 3 && head[0] == "/os:" && (head[1] == "+" || head[1] == "-")) {
            Request r;
            if (parse_request_line(message.head, r)) {
                if (head[1] == "+") requests[r.id] = r; else requests.erase(r.id);
                return;
            }
        }
        if (server_error(message)) return;
        if (head.size() >= 2 && head[0] == "/os:" && head[1].starts_with(".")) {
            std::string match_id; int sub = -1;
            if (!ggs::split_game_id(head[1], match_id, sub)) return;
            auto& chat = match_chat[match_id];
            chat.push_back(ggs::trim(message.head.substr(message.head.find(head[1]) + head[1].size())));
            for (const auto& line : message.body) chat.push_back(line);
            while (chat.size() > 300) chat.pop_front();
        }
    }
    Match& update_row(const ggs::MatchRow& row, bool new_active = false) {
        auto& m = ensure(row.id);
        if (new_active && m.closed_event) { m = Match{}; m.id = row.id; watches.erase(row.id); }
        m.type = row.type; m.flag = row.flag; m.listed = true;
        m.player[0] = row.player[0]; m.player[1] = row.player[1];
        return m;
    }
    bool may_play(const Match& m, int sub, bool online, int replay_ply = -1) const {
        if (!online || registered != true || replay_ply >= 0 || sub < 0 || sub >= m.games()) return false;
        const auto& g = m.game[sub];
        return g.active && !g.finished && !m.finished && m.clock_running(sub) && g.color_of(username) == g.pos.to_move;
    }
    std::optional<std::string> move_command(const Match& m, int sub, int cell, bool online, int replay_ply = -1) const {
        if (!may_play(m, sub, online, replay_ply)) return {};
        Position p = m.game[sub].pos;
        if (!apply_move(p, cell < 0, cell)) return {};
        return "t /os play " + m.game[sub].id + " " + coordinate(cell);
    }
};

inline std::string escaped_property(const std::string& s) {
    std::string r;
    for (char c : s) { if (c == '\\' || c == ']') r += '\\'; if (c != '\r' && c != '\n') r += c; }
    return r;
}
inline std::string export_ggf(const Game& g, const std::string& type) {
    if (!g.history_ok || g.history.empty()) return {};
    const auto& start = g.history.front();
    std::ostringstream out;
    out << "(;GM[Othello]PC[GGS / GGSB]PB[" << escaped_property(g.name[BLACK]) << "]PW[" << escaped_property(g.name[WHITE])
        << "]RB[" << g.rating[BLACK] << "]RW[" << g.rating[WHITE] << "]TI[00:00]TY[" << escaped_property(type) << "]BO[8 ";
    for (int i = 0; i < 64; ++i) { out << (at(start, i) == BLACK ? '*' : at(start, i) == WHITE ? 'O' : '-'); if (i % 8 == 7) out << ' '; }
    out << ' ' << (start.to_move == BLACK ? '*' : 'O') << ']';
    for (size_t i = 0; i < g.moves.size(); ++i) {
        const auto& move = g.moves[i];
        out << (g.history[i].to_move == BLACK ? "B[" : "W[") << coordinate(move.pass ? -1 : move.cell);
        if (move.has_eval) out << '/' << move.eval;
        out << ']';
    }
    if (g.finished) out << "RE[" << std::showpos << g.final_score() << std::noshowpos << ']';
    out << ")\n"; return out.str();
}
inline bool import_ggf(const std::string& text, Match& match, std::string& error) {
    if (text.size() > 1024 * 1024) { error = "棋譜ファイルが大きすぎます"; return false; }
    static const std::regex properties(R"(([A-Z]+)\[((?:\\.|[^\]\\])*)\])");
    Game g; Position p = initial_position(); std::string type = "8";
    for (auto it = std::sregex_iterator(text.begin(), text.end(), properties); it != std::sregex_iterator(); ++it) {
        auto key = (*it)[1].str(), value = (*it)[2].str();
        if (key == "PB" || key == "PW") {
            std::string name;
            for (size_t i = 0; i < value.size(); ++i) { if (value[i] == '\\' && i + 1 < value.size()) ++i; name += value[i]; }
            g.name[key == "PB" ? BLACK : WHITE] = name;
        }
        else if (key == "RB" || key == "RW") ggs::parse_double(value, g.rating[key == "RB" ? BLACK : WHITE]);
        else if (key == "TY") type = value;
        else if (key == "BO") {
            if (!g.history.empty()) { error = "開始局面の位置が不正です"; return false; }
            auto w = ggs::tokenize(value); std::string squares;
            if (w.size() >= 3) for (size_t i = 1; i + 1 < w.size(); ++i) squares += w[i];
            if (w.size() < 3 || w[0] != "8" || squares.size() != 64 || (w.back() != "*" && w.back() != "O")) { error = "8×8のGGF棋譜に対応しています"; return false; }
            p = {}; p.to_move = w.back() == "*" ? BLACK : WHITE;
            for (int i = 0; i < 64; ++i) {
                if (squares[i] == '*') p.black |= ggs::cell_bit(i);
                else if (squares[i] == 'O') p.white |= ggs::cell_bit(i);
                else if (squares[i] != '-') { error = "開始局面の石が不正です"; return false; }
            }
        } else if (key == "B" || key == "W") {
            if (g.history.empty()) g.history.push_back(p);
            ggs::MoveInfo move;
            if (!ggs::parse_move_token(value, move) || p.to_move != (key == "B" ? BLACK : WHITE)) { error = "手番または着手の書式が不正です"; return false; }
            move.ply = int(g.moves.size()) + 1; g.record_eval(p.to_move, move, &p);
            if (!apply_move(p, move.pass, move.cell)) { error = "棋譜に非合法手が含まれています"; return false; }
            g.moves.push_back(move); g.history.push_back(p);
        } else if (key == "RE") { g.has_official_result = ggs::parse_double(value.substr(0, value.find(':')), g.result_black); g.finished = g.has_official_result; }
    }
    if (g.name[BLACK].empty() || g.name[WHITE].empty() || !supported_type(type)) { error = "有効なオセロGGF棋譜ではありません"; return false; }
    if (g.history.empty()) g.history.push_back(p);
    g.pos = p; g.active = g.history_ok = true; g.ply = int(g.moves.size());
    g.last_cell = g.moves.empty() || g.moves.back().pass ? -1 : g.moves.back().cell;
    g.finished = g.finished || is_game_over(p); g.final_evals();
    match = {}; match.id = "local"; match.type = "8"; match.listed = false; match.watching = true;
    match.player[0] = g.name[BLACK]; match.player[1] = g.name[WHITE]; match.game[0] = std::move(g);
    match.finished = match.complete(); return true;
}
inline bool import_pair_ggf(const std::string& first, const std::string& second, Match& match, std::string& error) {
    Match a, b;
    if (!import_ggf(first, a, error) || !import_ggf(second, b, error)) return false;
    if (a.player[0] != b.player[1] || a.player[1] != b.player[0] || !(a.game[0].history.front() == b.game[0].history.front())) {
        error = "2局の選手または開始局面が一致しません"; return false;
    }
    match = std::move(a); match.type = "s8"; match.game[1] = std::move(b.game[0]);
    match.game[0].id = "local.0"; match.game[1].id = "local.1";
    match.finished = match.complete(); return true;
}
}
