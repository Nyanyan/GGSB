#pragma once
#include "model.hpp"
#include <limits>

namespace ggsb {
inline int64_t archive_number(const std::string& id) {
    if (id.size() < 2 || id[0] != '.') return -1;
    try { size_t used = 0; auto n = std::stoll(id.substr(1), &used); return used == id.size() - 1 && n >= 0 ? n : -1; }
    catch (...) { return -1; }
}
inline std::string csv_cell(const std::string& value) {
    std::string s = "\""; for (char c : value) { if (c == '"') s += '"'; s += c; } return s + '"';
}
struct RecordedResult {
    std::string key, scope, archive_id, match_id, ended, player[2], type, ggf_path;
    double result0 = 0;
    bool observed_live = false;
    int games() const { return type.starts_with("s") || type.starts_with("S") ? 2 : 1; }
    int index(const std::string& name) const { return player[0] == name ? 0 : player[1] == name ? 1 : -1; }
    double score(int i) const { return i == 0 ? result0 : -result0; }
};
inline bool parse_history(const ggs::Message& message, std::string& player, std::vector<RecordedResult>& rows) {
    auto head = ggs::tokenize(message.head);
    if (head.size() != 4 || head[0] != "/os:" || head[1] != "history") return false;
    int size = 0; if (!ggs::parse_int(head[2], size) || size < 0 || !token_safe(head[3])) return false;
    player = head[3]; rows.clear();
    for (const auto& line : message.body) {
        auto w = ggs::tokenize(line);
        // counter, day month year HH:mm:ss, rating/name, rating/name, result, type [flags]
        if (w.size() < 11 || archive_number(w[0]) < 0 || !supported_type(w[10])) continue;
        double rating, score;
        if (!ggs::parse_double(w[5], rating) || !ggs::parse_double(w[7], rating) || !ggs::parse_double(w[9], score) || std::abs(score) > 64) continue;
        RecordedResult row; row.archive_id = w[0]; row.type = w[10]; row.result0 = score;
        row.player[0] = w[6]; row.player[1] = w[8]; row.ended = w[1] + " " + w[2] + " " + w[3] + " " + w[4] + " (server)";
        if (row.index(player) >= 0 && token_safe(row.player[0]) && token_safe(row.player[1])) rows.push_back(std::move(row));
    }
    return true;
}
struct TrackingSession {
    std::string id, scope, player, started;
    bool active = true, baseline_ready = false;
    int64_t baseline = -1;
    std::set<std::string> included;
};
struct OpponentStats {
    std::string opponent;
    int wins = 0, draws = 0, losses = 0, boards = 0, synchro = 0;
    double margin_sum = 0;
    int matches() const { return wins + draws + losses; }
    double win_rate() const { return matches() ? (wins + draws * 0.5) / matches() : 0; }
    double average_margin() const { return boards ? margin_sum / boards : 0; }
    void add(const RecordedResult& result, int idx) {
        const double score = result.score(idx);
        if (score > 0.00001) ++wins; else if (score < -0.00001) ++losses; else ++draws;
        boards += result.games(); synchro += result.games() == 2;
        margin_sum += score * result.games();
    }
};
class PlayerRecords {
public:
    std::map<std::string, RecordedResult> results;
    std::vector<TrackingSession> sessions;
    TrackingSession& start(const std::string& id, const std::string& scope, const std::string& player, const std::string& time) {
        for (auto& session : sessions) if (session.scope == scope && session.player == player) session.active = false;
        sessions.push_back({id, scope, player, time, true, false, -1, {}}); return sessions.back();
    }
    TrackingSession* find(const std::string& id) {
        for (auto& session : sessions) if (session.id == id) return &session;
        return nullptr;
    }
    bool wants(const std::string& scope, const std::string& player) const {
        for (const auto& session : sessions) if (session.active && session.scope == scope && session.player == player) return true;
        return false;
    }
    bool live(RecordedResult row) {
        if (!supported_type(row.type) || !std::isfinite(row.result0) || std::abs(row.result0) > 64) return false;
        auto old = results.find(row.key);
        if (old != results.end() && old->second.observed_live) return false;
        if (old != results.end() && row.ggf_path.empty()) row.ggf_path = old->second.ggf_path;
        row.observed_live = true;
        for (auto& session : sessions) if (session.active && session.scope == row.scope && row.index(session.player) >= 0) session.included.insert(row.key);
        results.insert_or_assign(row.key, std::move(row)); return true;
    }
    void history(const std::string& scope, const std::string& player, std::vector<RecordedResult> rows) {
        int64_t largest = -1;
        for (const auto& row : rows) largest = std::max(largest, archive_number(row.archive_id));
        for (auto& session : sessions) if (session.active && session.scope == scope && session.player == player) {
            if (!session.baseline_ready) { session.baseline = largest; session.baseline_ready = true; }
            else for (const auto& row : rows) if (archive_number(row.archive_id) > session.baseline) session.included.insert(scope + "|" + row.archive_id);
        }
        for (auto& row : rows) {
            row.scope = scope; row.key = scope + "|" + row.archive_id;
            if (!results.count(row.key)) results.emplace(row.key, std::move(row));
        }
    }
    std::vector<OpponentStats> stats(const TrackingSession& session) const {
        std::map<std::string, OpponentStats> opponents;
        for (const auto& key : session.included) {
            auto it = results.find(key); if (it == results.end()) continue;
            const auto& r = it->second; const int idx = r.index(session.player); if (idx < 0 || r.scope != session.scope) continue;
            auto& stat = opponents[r.player[1 - idx]]; stat.opponent = r.player[1 - idx]; stat.add(r, idx);
        }
        std::vector<OpponentStats> out; for (auto& [name, stat] : opponents) out.push_back(stat);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.matches() != b.matches() ? a.matches() > b.matches() : a.opponent < b.opponent; });
        return out;
    }
    std::string history_csv(const std::string& scope, const std::string& player) const {
        std::ostringstream out; out << "archive_id,match_id,ended,player,opponent,type,boards,result_per_board,win_points,ggf_path\n";
        for (const auto& [key, r] : results) if (r.scope == scope && r.index(player) >= 0) {
            const int idx = r.index(player); const double score = r.score(idx);
            out << csv_cell(r.archive_id) << ',' << csv_cell(r.match_id) << ',' << csv_cell(r.ended) << ',' << csv_cell(player) << ',' << csv_cell(r.player[1 - idx]) << ','
                << csv_cell(r.type) << ',' << r.games() << ',' << score << ',' << (score > 0 ? 1 : score < 0 ? 0 : 0.5) << ',' << csv_cell(r.ggf_path) << '\n';
        }
        return out.str();
    }
    std::string stats_csv(const TrackingSession& session) const {
        std::ostringstream out; out << "session_id,started,player,opponent,matches,wins,draws,losses,win_rate,boards,synchro_matches,average_margin_per_board\n";
        for (const auto& stat : stats(session)) out << csv_cell(session.id) << ',' << csv_cell(session.started) << ',' << csv_cell(session.player) << ',' << csv_cell(stat.opponent) << ','
            << stat.matches() << ',' << stat.wins << ',' << stat.draws << ',' << stat.losses << ',' << stat.win_rate() << ',' << stat.boards << ',' << stat.synchro << ',' << stat.average_margin() << '\n';
        return out.str();
    }
    std::string serialize() const {
        std::ostringstream out; out << "GGSB_RECORDS 1\n" << std::setprecision(17);
        for (const auto& [key, r] : results) out << "R " << std::quoted(key) << ' ' << std::quoted(r.scope) << ' ' << std::quoted(r.archive_id) << ' ' << std::quoted(r.match_id) << ' '
            << std::quoted(r.ended) << ' ' << std::quoted(r.player[0]) << ' ' << std::quoted(r.player[1]) << ' ' << std::quoted(r.type) << ' ' << r.result0 << ' ' << std::quoted(r.ggf_path) << ' ' << r.observed_live << '\n';
        for (const auto& s : sessions) {
            out << "S " << std::quoted(s.id) << ' ' << std::quoted(s.scope) << ' ' << std::quoted(s.player) << ' ' << std::quoted(s.started) << ' ' << s.active << ' ' << s.baseline_ready << ' ' << s.baseline << '\n';
            for (const auto& key : s.included) out << "I " << std::quoted(s.id) << ' ' << std::quoted(key) << '\n';
        }
        return out.str();
    }
    bool deserialize(const std::string& data) {
        if (data.size() > 32 * 1024 * 1024) return false;
        std::istringstream in(data); std::string header; std::getline(in, header); if (header != "GGSB_RECORDS 1") return false;
        PlayerRecords loaded; std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream row(line); char kind; row >> kind;
            if (kind == 'R') {
                RecordedResult r;
                if (!(row >> std::quoted(r.key) >> std::quoted(r.scope) >> std::quoted(r.archive_id) >> std::quoted(r.match_id) >> std::quoted(r.ended) >> std::quoted(r.player[0]) >> std::quoted(r.player[1]) >> std::quoted(r.type) >> r.result0 >> std::quoted(r.ggf_path) >> r.observed_live) || !supported_type(r.type) || !std::isfinite(r.result0) || std::abs(r.result0) > 64) return false;
                loaded.results.emplace(r.key, std::move(r));
            } else if (kind == 'S') {
                TrackingSession s;
                if (!(row >> std::quoted(s.id) >> std::quoted(s.scope) >> std::quoted(s.player) >> std::quoted(s.started) >> s.active >> s.baseline_ready >> s.baseline) || !token_safe(s.player)) return false;
                loaded.sessions.push_back(std::move(s));
            } else if (kind == 'I') {
                std::string id, key; if (!(row >> std::quoted(id) >> std::quoted(key))) return false;
                auto* s = loaded.find(id); if (!s || !loaded.results.count(key)) return false; s->included.insert(key);
            } else return false;
        }
        *this = std::move(loaded); return true;
    }
};
}
