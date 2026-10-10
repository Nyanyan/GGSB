#pragma once
#include "application.hpp"

namespace ggsb {
inline void Application::seed_demo_records() {
    auto& session = demo_records.start("demo-session", "DEMO", "Kurogane", "2026-10-09 18:00:00 [DEMO]");
    session.baseline_ready = true; selected_session = session.id;
    const char* opponents[] = {"shiro", "parity", "Mobility", "corner", "Tempo"};
    const double scores[] = {16, -8, 0, 24, 12, -2, 0, 10, 28, -16, 6, 0, 18, 4, -4, 22, 14, -10};
    for (int i = 0; i < 18; ++i) {
        RecordedResult r; r.scope = "DEMO"; r.archive_id = "." + std::to_string(90000 + i); r.key = r.scope + "|" + r.archive_id;
        r.player[0] = "Kurogane"; r.player[1] = opponents[i % 5]; r.type = i % 3 ? "s8r14" : "8";
        r.result0 = scores[i]; r.ended = "2026-10-09 18:" + std::string(i < 10 ? "0" : "") + std::to_string(i) + ":00 [DEMO]";
        demo_records.live(r);
    }
}
inline void Application::select_player(const std::string& player) {
    tracking_player.value = ui::str(player); tracking_player.cursor = tracking_player.value.size();
    selected_session.clear(); records_offset = 0; page = 6;
    for (const auto& session : record_book().sessions) if (session.scope == record_scope() && session.player == player) selected_session = session.id;
    if (online()) send("t /os history " + player);
}
inline bool Application::save_records() {
    if (!records_writable) { notify(records_status); return false; }
    FileSystem::CreateDirectories(FileSystem::ParentPath(records_file));
    {
        TextWriter writer{records_file + U".tmp", TextEncoding::UTF8_NO_BOM};
        if (!writer) { records_status = ui::tr(U"記録を書き込めませんでした"); notify(records_status); return false; }
        writer.write(ui::str(records.serialize()));
    }
    if (FileSystem::Exists(records_file)) FileSystem::Copy(records_file, records_file + U".bak", CopyOption::OverwriteExisting);
    if (!FileSystem::Copy(records_file + U".tmp", records_file, CopyOption::OverwriteExisting)) {
        records_status = ui::tr(U"記録の保存に失敗しました"); notify(records_status); return false;
    }
    return true;
}
inline void Application::start_tracking() {
    std::string player = tracking_player.value.toUTF8();
    if (!token_safe(player) || player == "*") { notify(ui::tr(U"プレイヤー名を入力してください")); return; }
    if (mode != Mode::Demo && !online()) { notify(ui::tr(U"GGSに接続してから記録を開始してください")); return; }
    if (mode != Mode::Demo && !records_writable) { notify(records_status); return; }
    const std::string id = DateTime::Now().format(U"yyyyMMdd-HHmmss").toUTF8() + "-" + std::to_string(now());
    auto& session = record_book().start(id, record_scope(), player, DateTime::Now().format(U"yyyy-MM-dd HH:mm:ss").toUTF8());
    selected_session = session.id; records_offset = 0;
    sessions_offset = 0;
    if (mode == Mode::Demo) session.baseline_ready = true;
    else { save_records(); send("t /os history " + player); last_history_refresh = now(); tick_records(); }
    records_status = ui::tr(U"記録を開始しました · 新しい区間として保存します"); notify(records_status);
}
inline FilePath Application::save_match_files(const Match& match, const String& base) {
    for (int i = 0; i < match.games(); ++i) if (!match.game[i].active || export_ggf(match.game[i], match.type).empty()) return {};
    FileSystem::CreateDirectories(FileSystem::ParentPath(base));
    for (int i = 0; i < match.games(); ++i) {
        TextWriter writer{base + U"_" + Format(i) + U".ggf", TextEncoding::UTF8_NO_BOM};
        if (!writer) return {};
        writer.write(ui::str(export_ggf(match.game[i], match.type)));
    }
    return base + U"_0.ggf";
}
inline void Application::receive_records(const ggs::Message& message) {
    if (mode != Mode::Live) return;
    const auto scope = server_scope();
    std::string player; std::vector<RecordedResult> history;
    if (parse_history(message, player, history)) {
        const size_t n = history.size(); records.history(scope, player, std::move(history));
        records_status = ui::str(player) + ui::tr(U" · サーバー履歴 ") + Format(n) + ui::tr(U"件を保存しました"); save_records();
        return;
    }
    ggs::MatchEvent event;
    if (!ggs::parse_match_event(message, event) || !supported_type(event.row.type)) return;
    if (event.start) { closed_keys.erase(event.row.id); return; }
    if (!event.has_result || (!records.wants(scope, event.row.player[0]) && !records.wants(scope, event.row.player[1]))) return;
    RecordedResult r; r.scope = scope; r.match_id = event.row.id; r.archive_id = event.archive_id; r.type = event.row.type;
    r.player[0] = event.row.player[0]; r.player[1] = event.row.player[1]; r.result0 = event.result0;
    r.ended = DateTime::Now().format(U"yyyy-MM-dd HH:mm:ss").toUTF8();
    r.key = scope + "|" + (r.archive_id.empty() ? "live:" + std::to_string(epoch) + ":" + r.match_id + ":" + std::to_string(now()) : r.archive_id);
    if (closed_keys.count(r.match_id) && r.archive_id.empty()) r.key = closed_keys[r.match_id];
    closed_keys[r.match_id] = r.key;
    auto found = model.matches.find(r.match_id);
    if (found != model.matches.end()) {
        FilePath base = U"records/auto/" + DateTime::Now().format(U"yyyyMMdd-HHmmss") + U"_" + ui::str(r.archive_id.empty() ? r.match_id : r.archive_id).replace(U".", U"");
        r.ggf_path = save_match_files(found->second, base).toUTF8();
    }
    if (records.live(r)) { save_records(); records_status = ui::tr(U"終局を記録しました · ") + ui::str(r.player[0]) + U" vs " + ui::str(r.player[1]); }
}
inline void Application::tick_records() {
    if (!online()) return;
    const auto scope = server_scope();
    std::set<std::string> players;
    for (const auto& session : records.sessions) if (session.active && session.scope == scope) players.insert(session.player);
    if (now() - last_history_refresh > 45000 || last_history_refresh == 0) {
        for (const auto& player : players) send("t /os history " + player);
        last_history_refresh = now();
    }
    for (auto& [id, match] : model.matches) if (!match.finished && match.listed && !model.watches.count(id)
        && (players.count(match.player[0]) || players.count(match.player[1]))) {
        if (send("t /os watch + " + id)) { model.watches.insert(id); match.watch_ms = now(); }
    }
    bool saved = false;
    for (const auto& [id, key] : closed_keys) {
        auto r = records.results.find(key); auto m = model.matches.find(id);
        if (r == records.results.end() || m == model.matches.end() || !r->second.ggf_path.empty() || r->second.scope != scope) continue;
        auto path = save_match_files(m->second, U"records/auto/" + DateTime::Now().format(U"yyyyMMdd-HHmmss") + U"_" + ui::str(r->second.archive_id.empty() ? id : r->second.archive_id).replace(U".", U""));
        if (!path.empty()) { r->second.ggf_path = path.toUTF8(); saved = true; }
    }
    if (saved) save_records();
}
inline void Application::export_player_records() {
    auto& book = record_book(); auto* session = book.find(selected_session);
    const auto player = session ? session->player : tracking_player.value.toUTF8();
    const auto scope = session ? session->scope : record_scope();
    if (!token_safe(player)) { notify(ui::tr(U"プレイヤーを選んでください")); return; }
    // Filenames use a local timestamp; GGS names never become filesystem paths.
    const String base = U"records/player_" + DateTime::Now().format(U"yyyyMMdd-HHmmss"); FileSystem::CreateDirectories(U"records/");
    TextWriter history{base + U"_history.csv", TextEncoding::UTF8_WITH_BOM};
    if (!history) { notify(ui::tr(U"履歴の保存に失敗しました")); return; }
    history.write(ui::str(book.history_csv(scope, player)));
    if (session) {
        TextWriter stats{base + U"_stats.csv", TextEncoding::UTF8_WITH_BOM};
        if (!stats) { notify(ui::tr(U"集計の保存に失敗しました")); return; }
        stats.write(ui::str(book.stats_csv(*session)));
    }
    notify(session ? ui::tr(U"対局履歴と相手別の集計をCSVに保存しました · records/") : ui::tr(U"対局履歴をCSVに保存しました · records/"));
}
}
