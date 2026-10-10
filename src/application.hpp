#pragma once
#include "network.hpp"
#include "demo.hpp"
#include "ui.hpp"
#include "tracking.hpp"
#include "accounts.hpp"
#include "console.hpp"
#include "sound_settings.hpp"

namespace ggsb {
using namespace s3d;
class Application {
public:
    ClientModel model;
    ui::Painter p;
    std::unique_ptr<Connection> connection;
    Demo demo;
    enum class Mode { Offline, Demo, Live, Record };
    static constexpr int ConnectionPage = 7;
    static constexpr int ChatPage = 8;
    static constexpr int PrivateChatVisible = 5;
    Mode mode = Mode::Offline;
    int page = ConnectionPage, lobby_tab = 0, list_offset = 0, move_offset = 0, active_board = 0, chat_offset = 0, graph_board = -1;
    ConsoleBuffer console_view;
    struct ChatView {
        ConsoleBuffer output;
        uint64_t origin = 0;
        std::vector<std::pair<uint64_t, int>> row_counts;
    };
    std::map<std::string, ChatView> chat_views;
    std::map<std::string, ui::Field> chat_drafts;
    int chat_tabs_offset = 0, watched_offset = 0;
    bool move_follow_tail = true;
    bool console_drag_vertical = false, console_drag_horizontal = false;
    double console_drag_offset = 0, console_autoscroll_due = 0;
    bool match_settings_sent = false;
    std::map<std::string, int> pending_moves;
    std::map<std::string, int> cursors;
    std::map<std::string, int> graph_cursors;
    bool graph_dragging = false;
    bool playback = false, legal_hints = true, sound = true, animations = true, flip_board = false;
    bool open_for_matches = true, rated = true, save_login = false;
    uint64_t last_playback = 0;
    int epoch = 0;
    std::string connected_scope;
    ui::Field host{U"skatgame.net"}, port{U"5000"}, username, password, filter, command, chat_input;
    ui::Field account_label;
    AccountVault accounts;
    std::string selected_account, account_error;
    int accounts_offset = 0;
    ui::Field chat_target{U"/os"}, opponent, my_clock{U"15:00/00:02/00:00"}, their_clock{U"15:00/00:02/00:00"}, random_discs{U"14"}, score_input{U"0"};
    bool synchro = true, random_start = true;
    int color_preference = 0;
    std::string modal, confirmation_command;
    String confirmation_title, confirmation_description, toast;
    double toast_until = 0;
    Audio click_sound;
    Audio chat_sound;
    SoundSettings audio;
    FilePath last_export;
    PlayerRecords records, demo_records;
    ui::Field tracking_player;
    std::string selected_session;
    int records_tab = 0, records_offset = 0, mosaic_offset = 0, sessions_offset = 0;
    uint64_t last_history_refresh = 0;
    std::map<std::string, std::string> closed_keys;
    String records_status;
    bool records_writable = true;
    FilePath records_file = U"records/players.ggsb";

    Application() {
        INI settings{U"settings.ini"};
        host.value = settings.getOr<String>(U"connection.host", U"skatgame.net");
        port.value = settings.getOr<String>(U"connection.port", U"5000");
        username.value = settings.getOr<String>(U"connection.username", U"");
        legal_hints = settings.getOr<bool>(U"view.legal_hints", true);
        sound = settings.getOr<bool>(U"view.sound", true);
        audio.set_volume(settings.getOr<int>(U"audio.volume", 100));
        audio.chat_enabled = settings.getOr<bool>(U"audio.chat_enabled", false);
        if (settings.hasSection(U"chat_sound")) for (const auto& entry : settings.getSection(U"chat_sound").keys) {
            const auto key = entry.name.toUTF8();
            if (SoundSettings::valid_key(key)) if (auto enabled = ParseOpt<bool>(entry.value)) audio.chats[key] = *enabled;
        }
        animations = settings.getOr<bool>(U"view.animations", true);
        ui::language_pack().select(settings.getOr<String>(U"view.language", U"en"));
        if (accounts.load(account_error)) if (const auto* account = accounts.find(accounts.preferred())) select_account(account->id);
        Wave wave(4410, Arg::sampleRate = 44100);
        for (size_t i = 0; i < wave.size(); ++i) {
            double t = i / 44100.0;
            const double attack = Min(1.0, t / 0.001);
            float sample = float(0.55 * attack * std::exp(-t * 48) * (0.72 * std::sin(2 * Math::Pi * 760 * t) + 0.28 * std::sin(2 * Math::Pi * 1470 * t)));
            wave[i] = WaveSample{sample, sample};
        }
        click_sound = Audio{std::move(wave)};
        Wave notification(12348, Arg::sampleRate = 44100);
        for (size_t i = 0; i < notification.size(); ++i) {
            const double t = i / 44100.0, note = t < 0.14 ? t : t - 0.14;
            const double envelope = Min(1.0, note / 0.006) * Min(1.0, (0.14 - note) / 0.035);
            const float sample = float(0.28 * envelope * std::sin(2 * Math::Pi * (t < 0.14 ? 880 : 1174.66) * note));
            notification[i] = WaveSample{sample, sample};
        }
        chat_sound = Audio{std::move(notification)};
        TextReader stored{records_file};
        if (stored && !records.deserialize(stored.readAll().toUTF8())) {
            TextReader backup{records_file + U".bak"};
            if (backup && records.deserialize(backup.readAll().toUTF8())) records_status = ui::tr(U"記録をバックアップから復元しました");
            else { records_writable = false; records_status = ui::tr(U"記録ファイルを読めません。既存ファイルを保護するため、自動保存を止めています。"); }
        }
        p.focus = username.value.empty() ? "username" : "password";
    }
    uint64_t now() const { return Time::GetMillisec(); }
    bool online() const { return mode == Mode::Live && connection && connection->online(); }
    bool may_match() const { return online() && model.registered == true; }
    int cursor() const { auto it = cursors.find(model.selected); return it == cursors.end() ? -1 : it->second; }
    int cursor_for(int sub) const {
        const auto match = model.matches.find(model.selected);
        if (cursor() < 0 || match == model.matches.end()) return -1;
        if (auto it = graph_cursors.find(model.selected); it != graph_cursors.end()) return match->second.game[sub].ply_at_placement(it->second);
        return std::min(cursor(), match->second.game[sub].ply);
    }
    int graph_cursor(const Match& m, int sub) const {
        if (cursor() < 0) return -1;
        if (auto it = graph_cursors.find(m.id); it != graph_cursors.end()) return it->second;
        return m.game[sub < 0 ? active_board : sub].placed_at(cursor_for(sub < 0 ? active_board : sub)).value_or(0);
    }
    int shown_discs(const Match& m, int idx) const {
        int count = 0;
        for (int sub = 0; sub < m.games(); ++sub) {
            const auto& g = m.game[sub]; const int color = g.color_of(m.player[idx]), at_ply = cursor_for(sub);
            if (g.active && color >= 0) count += disc_count(at_ply >= 0 && g.history_ok ? g.history[at_ply] : g.pos, color);
        }
        return count;
    }
    void latest() { cursors[model.selected] = -1; graph_cursors.erase(model.selected); playback = false; graph_dragging = false; }
    Match* selected() { auto it = model.matches.find(model.selected); return it == model.matches.end() ? nullptr : &it->second; }
    void notify(String message) { toast = std::move(message); toast_until = Scene::Time() + 4.5; }
    bool send(const std::string& cmd) {
        if (!online() || !connection->send(cmd)) { notify(ui::tr(U"GGSに接続すると利用できます")); return false; }
        model.append_console({"> " + cmd, {}});
        return true;
    }
    void save_settings() {
        INI settings;
        settings.write(U"connection", U"host", host.value); settings.write(U"connection", U"port", port.value);
        settings.write(U"view", U"legal_hints", legal_hints); settings.write(U"view", U"sound", sound); settings.write(U"view", U"animations", animations);
        settings.write(U"view", U"language", ui::language_pack().selected());
        settings.write(U"audio", U"volume", audio.volume); settings.write(U"audio", U"chat_enabled", audio.chat_enabled);
        for (const auto& [key, enabled] : audio.chats) settings.write(U"chat_sound", ui::str(key), enabled);
        if (!settings.save(U"settings.ini")) notify(ui::tr(U"設定を保存できませんでした"));
    }
    std::string sound_scope() const {
        if (mode == Mode::Demo) return "demo";
        if (mode == Mode::Record) return "record";
        if (!connected_scope.empty()) return connected_scope;
        return host.value.lowercased().toUTF8() + ":" + std::to_string(ParseOr<int>(port.value, 5000));
    }
    bool chat_sound_enabled(const std::string& target) const { return audio.chat_enabled_for(sound_scope(), target); }
    void set_chat_sound(const std::string& target, bool enabled) { audio.set_chat(sound_scope(), target, enabled); save_settings(); }
    bool chat_sound_due() const { return audio.chat_due(sound_scope(), model.chats.received); }
    void draw_chat_sound_toggle(const std::string& target, RectF area) {
        bool enabled = chat_sound_enabled(target);
        if (p.toggle(area, ui::tr(audio.chat_enabled ? U"通知音" : U"通知音（設定でオフ）"), enabled)) set_chat_sound(target, enabled);
    }
    void set_language(const String& language, bool persist = true) {
        ui::language_pack().select(language);
        if (persist) save_settings();
    }
    void draw_language_picker(RectF region) {
        const double width = (region.w - 12) / 2;
        if (p.button({region.x, region.y, width, region.h}, U"English", ui::language_pack().selected() == U"en")) set_language(U"en");
        if (p.button({region.x + width + 12, region.y, width, region.h}, U"日本語", ui::language_pack().selected() == U"ja")) set_language(U"ja");
    }
    void start_demo() {
        p.focus.clear(); modal.clear();
        connection.reset(); mode = Mode::Demo; demo.start(model, now());
        reset_chat_views();
        seed_demo_records();
        cursors.clear(); graph_cursors.clear(); pending_moves.clear(); playback = false; active_board = 0; graph_board = -1; move_offset = 0; move_follow_tail = true; console_view = {}; page = 0;
    }
    void show_connection() { page = ConnectionPage; modal.clear(); p.focus = username.value.empty() ? "username" : "password"; }
    void cancel_connection() {
        connection.reset(); mode = Mode::Offline; model = {}; epoch = 0; match_settings_sent = false;
        reset_chat_views();
        pending_moves.clear(); cursors.clear(); graph_cursors.clear(); playback = false;
        show_connection(); toast.clear();
    }
    void connect() {
        int port_number = 0;
        if (!token_safe(host.value.toUTF8()) || !token_safe(username.value.toUTF8()) || !ggs::parse_int(port.value.toUTF8(), port_number)
            || port_number <= 0 || port_number > 65535 || password.value.contains(U'\n') || password.value.contains(U'\r')) {
            notify(ui::tr(U"ホスト・ポート・ユーザー名を確認してください")); return;
        }
        if (save_login && !save_account()) return;
        if (const auto* account = accounts.find(selected_account); account && account->host == host.value.lowercased().toUTF8() && account->port == port_number && account->username == username.value.toUTF8()) {
            if (!accounts.prefer(selected_account, account_error)) notify(ui::tr(ui::str(account_error)));
        }
        connection.reset(); model = {}; model.username = username.value.toUTF8(); mode = Mode::Live;
        reset_chat_views();
        port.value = Format(port_number);
        connected_scope = host.value.lowercased().toUTF8() + ":" + std::to_string(port_number);
        closed_keys.clear(); last_history_refresh = 0; selected_session.clear();
        cursors.clear(); graph_cursors.clear(); pending_moves.clear(); playback = false; epoch = 0; match_settings_sent = false; graph_board = -1; console_view = {}; save_settings();
        FileSystem::CreateDirectories(U"logs/");
        std::string path = (U"logs/ggsb_" + DateTime::Now().format(U"yyyyMMdd-HHmmss") + U".log").toUTF8();
        connection = std::make_unique<Connection>(host.value.toUTF8(), port_number, username.value.toUTF8(), password.value.toUTF8(), path);
        connection->start(); page = ConnectionPage; modal.clear(); p.focus.clear(); notify(ui::tr(U"接続を開始しました"));
    }
    void refresh_users() { send("t /os who 8 a"); send("t /os who 8r a"); }
    void refresh() { send("t /os match"); refresh_users(); send("t /os request"); }
    void show_player_info(const std::string& name, bool game_stats = false) {
        if (auto info_request = player_info_command(name, game_stats); info_request && send(*info_request)) { page = 3; p.focus.clear(); }
    }
    void reset_chat_views() { chat_views.clear(); chat_drafts.clear(); chat_tabs_offset = 0; chat_offset = 0; }
    void open_chat(const std::string& target) {
        if (!model.chats.open(target)) return;
        page = ChatPage; modal.clear(); p.focus = "conversation-input"; p.hit_field = true;
        console_drag_vertical = console_drag_horizontal = false;
        auto it = std::find(model.chats.order.begin(), model.chats.order.end(), target);
        if (it != model.chats.order.end() && it >= model.chats.order.begin() + 2) {
            int idx = int(it - model.chats.order.begin()) - 2;
            if (idx < chat_tabs_offset) chat_tabs_offset = idx;
            else if (idx >= chat_tabs_offset + PrivateChatVisible) chat_tabs_offset = idx - PrivateChatVisible + 1;
        }
    }
    void close_chat(const std::string& target) {
        const bool active = page == ChatPage && model.chats.selected == target;
        model.chats.close(target);
        chat_tabs_offset = Clamp(chat_tabs_offset, 0, ListScroll::maximum(int(model.chats.order.size()) - 2, PrivateChatVisible));
        if (active) { page = 1; p.focus.clear(); console_drag_vertical = console_drag_horizontal = false; }
    }
    void submit_conversation() {
        const auto target = model.chats.selected;
        auto& draft = chat_drafts[target];
        auto cmd = conversation_command(target, draft.value.toUTF8());
        if (!cmd) { if (!draft.value.empty()) notify(ui::tr(U"メッセージが長すぎます")); return; }
        if (!conversation_target(target)) return;
        if (send(*cmd)) {
            model.chats.append(target, model.username, draft.value.toUTF8(), true);
            draft = {}; chat_views[target].output.follow_tail = true;
        }
    }
    void watch(const std::string& id) {
        model.selected = id; cursors[id] = -1; graph_cursors.erase(id); active_board = 0; graph_board = -1; move_offset = 0; move_follow_tail = true; page = 0;
        if (mode == Mode::Demo) { model.begin_watching(id, now()); return; }
        if (send("t /os watch + " + id)) model.begin_watching(id, now());
    }
    bool stop_watching(const std::string& id) {
        if (!model.can_stop_watching(id)) return false;
        if (online() && !send("t /os watch - " + id)) return false;
        const bool active = model.selected == id;
        if (!model.stop_watching(id)) return false;
        cursors.erase(id); graph_cursors.erase(id);
        if (active) { playback = graph_dragging = false; active_board = 0; move_offset = 0; move_follow_tail = true; }
        return true;
    }
    std::vector<Match*> watched_matches() {
        std::vector<Match*> matches;
        for (auto& [id, m] : model.matches) if (m.watching || model.watches.count(id)) matches.push_back(&m);
        std::stable_sort(matches.begin(), matches.end(), [](const auto* a, const auto* b) { return a->finished < b->finished; });
        return matches;
    }
    bool move_sound_due() {
        if (!sound || audio.gain() == 0) return false;
        if (page == 0) return cursor() < 0 && model.moved_matches.count(model.selected);
        if (page == 5) {
            const auto matches = watched_matches();
            const int first = Clamp(mosaic_offset / 4, 0, ListScroll::maximum((int(matches.size()) + 3) / 4, 1)) * 4;
            for (int slot = first; slot < Min(first + 4, int(matches.size())); ++slot)
                if (model.moved_matches.count(matches[slot]->id)) return true;
        }
        return false;
    }
    void choose_match(const std::string& id) { model.selected = id; active_board = 0; graph_board = -1; move_offset = 0; move_follow_tail = true; playback = false; }
    void seek(int ply) {
        if (auto* m = selected()) {
            bool complete_history = true;
            for (int i = 0; i < m->games(); ++i) complete_history &= m->game[i].active && m->game[i].history_ok;
            if (!complete_history) { notify(ui::tr(U"棋譜の履歴を受信中です")); return; }
            cursors[m->id] = std::clamp(ply, 0, m->max_ply()); graph_cursors.erase(m->id); playback = false;
        }
    }
    void seek_graph(int count) {
        if (auto* m = selected()) {
            seek(0);
            if (cursor() < 0) return;
            for (int sub = 0; sub < m->games(); ++sub) if (!m->game[sub].active || !m->game[sub].history_ok) return;
            count = std::clamp(count, 0, m->max_placements(graph_board));
            graph_cursors[m->id] = count;
            cursors[m->id] = m->game[active_board].ply_at_placement(count);
        }
    }
    void drag_graph(int count) {
        if (auto* m = selected()) {
            if (count >= m->max_placements(graph_board)) latest();
            else seek_graph(count);
        }
    }
    void step_replay(int direction) {
        if (auto* m = selected()) {
            if (graph_cursors.count(m->id)) seek_graph(graph_cursor(*m, graph_board) + direction);
            else seek(cursor() < 0 ? m->max_ply() + std::min(0, direction) : cursor() + direction);
        }
    }
    void play_cell(Match& m, int sub, int cell) {
        auto cmd = model.move_command(m, sub, cell, online(), cursor());
        if (pending_moves.count(m.game[sub].id)) return;
        if (cmd && send(*cmd)) pending_moves[m.game[sub].id] = m.game[sub].ply;
    }
    void tick() {
        if (mode == Mode::Demo) demo.tick(model, now());
        if (connection) {
            const auto messages = connection->poll();
            if (online() && connection->epoch() != epoch) {
                if (!epoch) { page = 1; modal.clear(); p.focus.clear(); lobby_tab = 0; list_offset = 0; }
                epoch = connection->epoch(); model.registered.reset(); match_settings_sent = false;
                last_history_refresh = 0; pending_moves.clear();
                // Connection already requested fresh lobby snapshots and subscribed to match notifications.
                for (const auto& id : model.watches) if (!model.ensure(id).finished) { send("t /os watch + " + id); model.ensure(id).watch_ms = now(); }
            }
            for (const auto& message : messages) {
                ggs::BoardMessage board;
                if (ggs::parse_board_message(message, board)) {
                    auto found = pending_moves.find(board.game_id);
                    if (found != pending_moves.end() && !board.moves.empty() && board.moves.back().ply > found->second) pending_moves.erase(found);
                }
                model.receive(message, now());
                receive_records(message);
                if (!unregistered_notice(message) && server_error(message)) { pending_moves.clear(); notify(ui::str(message.head)); }
            }
            if (may_match() && !match_settings_sent) {
                match_settings_sent = true;
                send(std::string("t /os open ") + (open_for_matches ? "1" : "0"));
                send(std::string("t /os rated ") + (rated ? "+" : "-"));
            }
            tick_records();
            if (online()) for (const auto& id : model.watches) {
                auto& m = model.ensure(id); bool need = false;
                for (int i = 0; i < m.games(); ++i) need |= !m.game[i].active || !m.game[i].history_ok;
                if (need && !m.finished && now() - m.watch_ms > 6000) { send("t /os watch + " + id); m.watch_ms = now(); }
            }
        }
        if (playback && now() - last_playback > 650) {
            last_playback = now();
            if (auto* m = selected()) {
                if (graph_cursors.count(m->id)) {
                    const int next = graph_cursor(*m, graph_board) + 1;
                    if (next > m->max_placements(graph_board)) latest();
                    else { seek_graph(next); playback = true; }
                } else { int next = cursor() + 1; if (next > m->max_ply()) latest(); else cursors[m->id] = next; }
            }
        }
        if (move_sound_due()) click_sound.playOneShot(audio.gain());
        if (chat_sound_due()) chat_sound.playOneShot(audio.gain());
        model.moved_matches.clear();
        model.chats.received.clear();
    }
    void export_record() {
        last_export.clear();
        auto* m = selected(); if (!m) return;
        FileSystem::CreateDirectories(U"records/");
        String base = U"records/" + DateTime::Now().format(U"yyyyMMdd-HHmmss") + U"_" + ui::str(m->id).replace(U".", U"");
        for (int i = 0; i < m->games(); ++i) {
            auto content = export_ggf(m->game[i], m->type);
            if (content.empty()) { notify(ui::tr(U"履歴がそろってから棋譜を保存できます")); return; }
        }
        for (int i = 0; i < m->games(); ++i) {
            TextWriter writer{base + U"_" + Format(i) + U".ggf", TextEncoding::UTF8_NO_BOM};
            if (!writer) { notify(ui::tr(U"棋譜の保存に失敗しました")); return; }
            writer.write(ui::str(export_ggf(m->game[i], m->type)));
        }
        TextWriter csv{base + U"_eval.csv", TextEncoding::UTF8_NO_BOM};
        csv.writeln(U"ply,player0_estimate,player1_estimate_in_player0_perspective");
        auto a = m->series(0), b = m->series(1);
        std::set<int> plies; for (const auto& v : a) plies.insert(v.first); for (const auto& v : b) plies.insert(v.first);
        for (int ply : plies) {
            String row = Format(ply) + U",";
            for (const auto& v : a) if (v.first == ply) row += ToString(v.second, 3);
            row += U",";
            for (const auto& v : b) if (v.first == ply) row += ToString(-v.second, 3);
            csv.writeln(row);
        }
        last_export = base + U"_0.ggf";
        notify(ui::tr(U"棋譜と評価値CSVを保存しました · records/"));
    }
    void load_record(const FilePath& path) {
        TextReader reader{path}; if (!reader) { notify(ui::tr(U"ファイルを開けませんでした")); return; }
        Match m; std::string error;
        if (!import_ggf(reader.readAll().toUTF8(), m, error)) { notify(ui::tr(ui::str(error))); return; }
        if (path.ends_with(U"_0.ggf") || path.ends_with(U"_1.ggf")) {
            FilePath base = path.substr(0, path.size() - 6);
            TextReader first{base + U"_0.ggf"}, second{base + U"_1.ggf"};
            if (first && second) {
                Match pair;
                if (import_pair_ggf(first.readAll().toUTF8(), second.readAll().toUTF8(), pair, error)) m = std::move(pair);
            }
        }
        p.focus.clear(); modal.clear();
        connection.reset(); mode = Mode::Record; model = {}; model.matches["local"] = m; model.selected = "local";
        reset_chat_views();
        cursors.clear(); graph_cursors.clear(); pending_moves.clear(); cursors["local"] = 0; playback = false; page = 0; active_board = 0; graph_board = -1; console_view = {};
        notify(ui::tr(U"棋譜を読み込みました"));
    }
    void open_record() { if (auto path = Dialog::OpenFile({FileFilter{ui::tr(U"GGF棋譜"), {U"ggf"}}})) load_record(*path); }
    void ask_confirmation(String title, String description, std::string cmd) {
        confirmation_title = std::move(title); confirmation_description = std::move(description); confirmation_command = std::move(cmd);
        modal = "confirm"; p.focus.clear();
    }
    void challenge(const String& who = U"") { if (!may_match()) return; opponent.value = who; modal = "challenge"; p.focus = "opponent"; }
    std::string challenge_command() const {
        std::string target = opponent.value.toUTF8(), clock = my_clock.value.toUTF8(), other = their_clock.value.toUTF8();
        if (!token_safe(target, true) || !token_safe(clock) || !token_safe(other)) return {};
        if (!valid_clock(clock) || !valid_clock(other)) return {};
        std::string type = synchro ? "s8" : "8";
        if (random_start) {
            int discs = 0; if (!synchro || !ggs::parse_int(random_discs.value.toUTF8(), discs) || discs < 4 || discs > 48) return {};
            type += "r" + std::to_string(discs);
        } else if (!synchro && color_preference) type += color_preference == 1 ? "b" : "w";
        return "t /os ask " + type + " " + clock + (clock != other ? " " + other : "") + (target.empty() ? "" : " " + target);
    }
    void submit_chat() {
        std::string target = chat_target.value.toUTF8(), value = chat_input.value.toUTF8();
        auto cmd = chat_command(target, value, model.selected);
        if (!cmd) { notify(ui::tr(U"宛先と対局を確認してください")); return; }
        const bool match_target = target == "/os" || target == "kibitz" || target == "whisper";
        if (!match_target && !conversation_target(target)) return;
        if (send(*cmd)) {
            if (match_target) {
                auto& chat = model.match_chat[model.selected]; chat.push_back(model.username + ": " + value);
                while (chat.size() > 300) chat.pop_front();
            } else model.chats.append(target, model.username, value, true);
            chat_input = {}; chat_offset = 0;
        }
    }
    bool verify_offline_workflow(String& result) {
        if (!ui::language_pack().available() || ui::language_pack().selected() != U"en" || ui::tr(U"ロビー") != U"Lobby") { result = U"FAIL: English default / language packs"; return false; }
        set_language(U"ja");
        INI japanese_settings{U"settings.ini"};
        if (ui::tr(U"ロビー") != U"ロビー" || japanese_settings.getOr<String>(U"view.language", U"") != U"ja") { result = U"FAIL: Japanese / language persistence"; return false; }
        set_language(U"en");
        if (ui::connection_status("切断 · 3秒後に再接続") != U"Disconnected · Retry in 3 s" || ui::tr(U"棋譜に非合法手が含まれています") != U"The game record contains an illegal move.") { result = U"FAIL: localized status / errors"; return false; }
        if (mode != Mode::Offline || page != ConnectionPage || !modal.empty() || !model.matches.empty() || may_match()) { result = U"FAIL: startup connection screen / no demo / match controls"; return false; }
        challenge(); if (!modal.empty()) { result = U"FAIL: offline challenge"; return false; }
        const auto old_username = username; username.value.clear();
        connect();
        if (page != ConnectionPage || connection || mode != Mode::Offline) { result = U"FAIL: invalid login stays on connection screen"; return false; }
        username = old_username; toast.clear();
        sound = false; start_demo();
        Match original = *selected(); export_record();
        if (last_export.empty() || !FileSystem::Exists(last_export)) { result = U"FAIL: export"; return false; }
        load_record(last_export);
        auto* loaded = selected();
        if (!loaded || loaded->games() != 2 || !(loaded->game[0].pos == original.game[0].pos) || !(loaded->game[1].pos == original.game[1].pos)) { result = U"FAIL: pair import"; return false; }
        seek(8);
        if (cursor() != 8 || model.move_command(*loaded, 0, 19, true, cursor())) { result = U"FAIL: replay guard"; return false; }
        start_demo(); const uint64_t graph_start = now();
        for (int step = 1; step <= 75; ++step) demo.tick(model, graph_start + step * 1700);
        auto* pair = selected();
        if (!pair || !pair->complete() || pair->x_max() != 50) { result = U"FAIL: graph initial-board limit"; return false; }
        if (!pair->winner() || !pair->game[0].winner() || !pair->game[1].winner() || pair->graph_range_for(0) != pair->graph_range_for(1)) { result = U"FAIL: winner / shared graph axis"; return false; }
        bool differing_passes = false;
        const int common = std::min(pair->game[0].placed(), pair->game[1].placed());
        for (int count = 0; count <= common; ++count) {
            seek_graph(count);
            differing_passes |= cursor_for(0) != cursor_for(1);
            for (int sub = 0; sub < 2; ++sub) if (pair->game[sub].placed_at(cursor_for(sub)) != count) { result = U"FAIL: graph replay pass alignment"; return false; }
            if (count < common) {
                step_replay(1);
                if (graph_cursor(*pair, -1) != count + 1) { result = U"FAIL: graph replay step"; return false; }
            }
        }
        if (!differing_passes) { result = U"FAIL: graph pass fixture"; return false; }
        seek_graph(0); playback = true; last_playback = now() - 700; tick();
        if (graph_cursor(*pair, -1) != 1 || !playback) { result = U"FAIL: graph playback"; return false; }
        seek(8);
        if (graph_cursors.count(pair->id) || cursor_for(0) != 8 || cursor_for(1) != 8) { result = U"FAIL: protocol-ply replay"; return false; }
        latest();
        if (cursor() >= 0 || graph_cursors.count(pair->id)) { result = U"FAIL: return to live graph"; return false; }
        for (int sub : {-1, 0, 1}) {
            graph_board = sub;
            const int end = pair->max_placements(sub);
            drag_graph(end - 1);
            if (cursor() < 0 || graph_cursor(*pair, sub) != end - 1) { result = U"FAIL: graph drag history"; return false; }
            playback = graph_dragging = true;
            drag_graph(end);
            if (cursor() >= 0 || graph_cursors.count(pair->id) || playback || graph_dragging || cursor_for(0) >= 0 || cursor_for(1) >= 0) { result = U"FAIL: latest graph position returns to live"; return false; }
            seek_graph(8); playback = graph_dragging = true;
            drag_graph(pair->x_max(sub));
            if (cursor() >= 0 || playback || graph_dragging) { result = U"FAIL: graph right edge returns to live"; return false; }
        }
        start_demo(); pair = selected();
        const int old_latest = pair->max_placements();
        drag_graph(old_latest - 1); drag_graph(old_latest);
        demo.tick(model, now() + 1700);
        if (cursor() >= 0 || pair->max_placements() <= old_latest || shown_discs(*pair, 0) != pair->total_discs(0)) { result = U"FAIL: graph live follows new moves"; return false; }
        const auto original_path = records_file;
        auto original_book = std::move(records); records = {};
        records_file = U"records/gui_smoke/players.ggsb";
        start_demo(); mode = Mode::Live;
        records.start("gui-session", server_scope(), "Kurogane", "GUI smoke");
        records.history(server_scope(), "Kurogane", {});
        for (const auto& head : {"/os: end .117.0 ( Kurogane vs. shiro ) +2.00", "/os: end .117.1 ( Kurogane vs. shiro ) -2.00", "/os: - match .117 2384 Kurogane 2297 shiro s8r14 R +0.00 .99999"}) {
            ggs::Message message{head, {}}; model.receive(message, now()); receive_records(message);
        }
        TextReader journal{records_file}; PlayerRecords restored;
        bool journal_ok = journal && restored.deserialize(journal.readAll().toUTF8());
        auto* session = restored.find("gui-session");
        auto stats = session ? restored.stats(*session) : std::vector<OpponentStats>{};
        bool stats_ok = stats.size() == 1 && stats[0].win_rate() == 0.5 && stats[0].average_margin() == 0;
        bool auto_ggf_ok = restored.results.size() == 1 && !restored.results.begin()->second.ggf_path.empty() && FileSystem::Exists(ui::str(restored.results.begin()->second.ggf_path));
        records = std::move(original_book); records_file = original_path;
        if (!journal_ok || !stats_ok || !auto_ggf_ok) { result = U"FAIL: player journal / stats / auto GGF"; return false; }
        if (!verify_account_workflow()) { result = U"FAIL: saved account workflow"; return false; }
        console_view.sync({U"ASCII: 0123456789", U"日本語の範囲選択", U"third line"}, 0);
        console_view.begin_selection({0, 7}); console_view.extend_selection({1, 3});
        if (console_view.selection_text() != U"0123456789\n日本語" || !p.mono || !p.mono_jp) { result = U"FAIL: console selection / fonts"; return false; }
        model.receive({".chat carol: public chat", {}}, now());
        model.receive({"bob: private chat", {}}, now());
        const auto watched = model.selected;
        open_chat("bob"); chat_drafts["bob"].value = U"下書き";
        if (page != ChatPage || model.chats.selected != "bob" || model.chats.threads.at("bob").unread || model.selected != watched) { result = U"FAIL: private chat navigation"; return false; }
        open_chat("carol"); chat_drafts["carol"].value = U"別の下書き";
        open_chat(".chat");
        if (model.chats.selected != ".chat" || chat_drafts["bob"].value != U"下書き" || chat_drafts["carol"].value != U"別の下書き" || !chat_drafts[".chat"].value.empty()) { result = U"FAIL: chat switching / separate drafts"; return false; }
        open_chat("bob"); close_chat("bob");
        if (page != 1 || model.chats.is_open("bob") || chat_drafts["bob"].value != U"下書き") { result = U"FAIL: close chat / preserve draft"; return false; }
        open_chat("bob");
        if (chat_drafts["bob"].value != U"下書き") { result = U"FAIL: reopen chat draft"; return false; }
        open_chat(".chat"); close_chat(".chat");
        if (page != 1 || !model.chats.is_open(".chat") || !model.chats.is_open(".tourney")) { result = U"FAIL: permanent channels"; return false; }
        start_demo(); sound = true;
        if (!click_sound || click_sound.samples() != 4410) { result = U"FAIL: placement audio resource"; return false; }
        model.moved_matches = {model.selected};
        if (!move_sound_due()) { result = U"FAIL: placement audio in match"; return false; }
        seek(1);
        if (move_sound_due()) { result = U"FAIL: live audio during review"; return false; }
        latest(); page = 5;
        if (!move_sound_due()) { result = U"FAIL: placement audio in mosaic"; return false; }
        page = 1;
        if (move_sound_due()) { result = U"FAIL: audio for hidden boards"; return false; }
        page = 5; sound = false;
        if (move_sound_due()) { result = U"FAIL: sound disabled"; return false; }
        choose_match(".117");
        if (!stop_watching(".117") || model.matches.at(".117").watching || !model.selected.empty()) { result = U"FAIL: stop watching active match"; return false; }
        demo.tick(model, now() + 1700);
        if (model.watches.count(".117") || model.matches.at(".117").watching) { result = U"FAIL: stopped watch reopened by delayed board"; return false; }
        watch(".117");
        if (!model.matches.at(".117").watching || model.selected != ".117") { result = U"FAIL: restart watching"; return false; }
        const auto original_audio = audio;
        audio.chat_enabled = true; audio.set_volume(37);
        set_chat_sound("bob", false); set_chat_sound(".chat", false);
        set_chat_sound(".tourney", true); set_chat_sound("/os", true);
        set_chat_sound("match:.117", false);
        audio.set_chat("other:5000", "bob", true); save_settings();
        {
            Application audio_reloaded;
            if (audio_reloaded.audio.volume != 37 || !audio_reloaded.audio.chat_enabled || audio_reloaded.audio.chats != audio.chats) { result = U"FAIL: audio settings / conversation persistence after restart"; return false; }
        }
        model.chats.received.clear();
        model.receive({"bob: muted conversation", {}}, now());
        model.receive({".chat bob: muted channel", {}}, now());
        if (chat_sound_due()) { result = U"FAIL: per-chat mute"; return false; }
        model.chats.received.clear();
        open_chat(".tourney"); model.receive({".tourney bob: incoming notification", {}}, now());
        if (!chat_sound_due() || !chat_sound || chat_sound.samples() != 12348) { result = U"FAIL: incoming chat notification"; return false; }
        audio.set_volume(0);
        if (chat_sound_due() || move_sound_due()) { result = U"FAIL: zero audio volume"; return false; }
        tick();
        if (!model.chats.received.empty()) { result = U"FAIL: chat notification repeated across frames"; return false; }
        audio = original_audio; save_settings();
        page = 4; set_language(U"ja");
        INI settings_after_login{U"settings.ini"};
        if (ui::tr(U"言語") != U"言語" || settings_after_login.getOr<String>(U"view.language", U"") != U"ja") { result = U"FAIL: settings language persistence"; return false; }
        set_language(U"en");
        cancel_connection();
        if (page != ConnectionPage || !modal.empty() || !model.matches.empty() || connection || mode != Mode::Offline) { result = U"FAIL: return to connection screen"; return false; }
        result = U"PASS: startup connection screen, invalid login stays on connection screen, no demo, offline match controls, console selection/fonts, chat navigation/close/reopen/drafts/permanent channels, placement audio resource and visible-board triggers/mute, chat notification/per-conversation mute/zero volume/settings persistence after restart, stop/restart watching with delayed updates, settings language persistence, GGF/CSV export, pair import, replay guard, graph limit/pass alignment/step/playback/live/right-edge drag/follow new moves, winner/shared graph axis, player journal, draw win rate/mean margin, automatic GGF, encrypted account save/select/reload/delete, return to connection screen";
        start_demo(); toast.clear(); return true;
    }
    void draw();
    void draw_shell();
    void draw_arena();
    void draw_board(Match& m, int sub, RectF region);
    void draw_graph(Match& m, RectF region);
    void draw_inspector(Match& m);
    void draw_lobby();
    void draw_users();
    void draw_console();
    void draw_text_output(ConsoleBuffer& view, RectF output, const std::string& focus_id);
    void draw_chat();
    void sync_console();
    void seed_console_preview();
    void draw_settings();
    void draw_connection_form(bool startup);
    void draw_modal();
    std::string server_scope() const { return mode == Mode::Live && connection ? connected_scope : host.value.lowercased().toUTF8() + ":" + port.value.toUTF8(); }
    PlayerRecords& record_book() { return mode == Mode::Demo ? demo_records : records; }
    std::string record_scope() const { return mode == Mode::Demo ? "DEMO" : server_scope(); }
    void seed_demo_records();
    void select_player(const std::string& player);
    void start_tracking();
    bool save_records();
    void receive_records(const ggs::Message& message);
    void tick_records();
    void export_player_records();
    FilePath save_match_files(const Match& match, const String& base);
    void draw_mosaic();
    void draw_records();
    void select_account(const std::string& id);
    void new_account();
    bool save_account();
    void delete_account();
    void draw_account_picker(RectF region);
    void seed_account_preview();
    void seed_scroll_preview();
    bool verify_account_workflow();
};
}
