#pragma once
#include "application.hpp"
#include "tracking_application.hpp"
#include "notebook_screens.hpp"
#include "account_application.hpp"
#include "console_screens.hpp"
#include "chat_screens.hpp"

namespace ggsb {
inline void Application::draw_shell() {
    using namespace ui;
    RectF{0, 0, 1524, 1000}.draw(background);
    RectF{0, 0, 230, 976}.draw(panel);
    Line{230, 0, 230, 976}.draw(1, line);
    p.label(U"GGSB", 28, 24, 24, text, true);
    const String nav[] = {ui::tr(U"ロビー"), ui::tr(U"対局・観戦"), ui::tr(U"複数対局"), ui::tr(U"プレイヤー"), ui::tr(U"記録・相手別成績"), ui::tr(U"コンソール"), ui::tr(U"接続・設定")};
    const int nav_pages[] = {1, 0, 5, 2, 6, 3, 4};
    for (int n = 0; n < 7; ++n) {
        const int i = nav_pages[n];
        RectF row{12, 76 + n * 40.0, 204, 38};
        if (page == i) RectF{12, row.y + 11, 3, 20}.draw(mint);
        p.label(nav[n], 16, 40, row.y + 7, page == i ? mint : text, page == i);
        if (!p.blocked && row.mouseOver()) Cursor::RequestStyle(CursorStyle::Hand);
        if (!p.blocked && p.dragging_list.empty() && row.leftClicked()) { page = i; list_offset = 0; }
    }
    Line{24, 370, 206, 370}.draw(1, line);
    p.label(ui::tr(U"チャット"), 12, 24, 386, muted, true);
    auto chat_tab = [&](const std::string& target, double y, bool global = false) {
        const bool active = page == ChatPage && model.chats.selected == target;
        const RectF row{12, y, 192, 34};
        if (active) RectF{12, y + 7, 3, 20}.draw(mint);
        const bool closable = target != ".chat" && target != ".tourney";
        p.clipped(str(target), 15, {24, y + 6, global ? 57.0 : closable ? 122.0 : 143.0, 23}, active ? mint : text, active);
        if (global) p.label(ui::tr(U"全体"), 12, 84, y + 8, muted);
        const auto& thread = model.chats.threads.at(target);
        if (thread.unread) p.mono(Format(thread.unread)).draw(11, Arg::topRight = Vec2{closable ? 173.0 : 200.0, y + 8}, mint);
        const bool closed = closable && p.button({180, y + 5, 25, 24}, U"×");
        if (closed) { close_chat(target); return; }
        if (!p.blocked && row.mouseOver()) Cursor::RequestStyle(CursorStyle::Hand);
        if (!p.blocked && p.dragging_list.empty() && row.leftClicked()) open_chat(target);
    };
    chat_tab(".chat", 412, true);
    chat_tab(".tourney", 450);
    const int private_count = int(model.chats.order.size()) - 2;
    p.scroll_list("chat-tabs", {12, 488, 192, 190}, {210, 488, 8, 190}, private_count, PrivateChatVisible, chat_tabs_offset);
    const std::vector<std::string> private_chats(model.chats.order.begin() + 2, model.chats.order.end());
    for (int n = 0; n < PrivateChatVisible && chat_tabs_offset + n < private_count; ++n) chat_tab(private_chats[chat_tabs_offset + n], 488 + n * 38);
    Line{24, 697, 206, 697}.draw(1, line);
    p.label(ui::tr(U"観戦中"), 12, 24, 716, muted, true);
    if (p.button({144, 709, 62, 27}, ui::tr(U"一覧"))) page = 5;
    std::vector<std::string> watched;
    for (const auto& [id, m] : model.matches) if (m.watching || model.watches.count(id)) watched.push_back(id);
    p.scroll_list("watched", {12, 746, 192, 192}, {210, 746, 8, 192}, int(watched.size()), 3, watched_offset);
    for (int n = 0; n < 3 && watched_offset + n < int(watched.size()); ++n) {
        const auto& id = watched[watched_offset + n]; auto& m = model.matches.at(id);
        const double y = 746 + n * 64;
        RectF r{12, y, 192, 60};
        if (id == model.selected) RectF{12, r.y + 4, 3, 52}.draw(mint);
        p.clipped(str(m.player[0]) + U" / " + str(m.player[1]), 13, {24, y + 3, 143, 23}, id == model.selected ? text : muted, true);
        const auto winner = m.winner();
        const bool finished = m.finished || m.complete();
        const String result = winner ? (*winner < 0 ? ui::tr(U"引き分け") : ui::tr(U"勝者 ") + str(m.player[*winner])) : finished ? ui::tr(U"結果なし") : ui::tr(U"対局中");
        p.clipped(result, 12, {24, y + 25, 143, 19}, winner ? mint : muted, finished);
        p.clipped((finished ? ui::tr(U"終局") + U" · " : U"") + (m.games() == 2 ? U"SYNC" : U"8×8") + U" · " + str(id), 9, {24, y + 46, 143, 15}, muted);
        const bool stopped = model.can_stop_watching(id) && p.button({180, y + 17, 25, 26}, U"×");
        if (stopped) { stop_watching(id); continue; }
        if (!p.blocked && p.dragging_list.empty() && r.leftClicked()) { choose_match(id); page = 0; }
    }
    if (watched.empty()) p.label(ui::tr(U"なし"), 13, 24, 753, muted);
    // Header
    const String titles[] = {ui::tr(U"対局・観戦"), ui::tr(U"ロビー"), ui::tr(U"プレイヤー"), ui::tr(U"コンソール"), ui::tr(U"接続・設定"), ui::tr(U"複数対局"), ui::tr(U"記録・相手別成績")};
    p.label(page == ChatPage ? ui::tr(U"チャット") : titles[page], 26, 256, 29, text, true);
    String status = mode == Mode::Demo ? ui::tr(U"OFFLINE DEMO") : mode == Mode::Record ? ui::tr(U"RECORD REVIEW") : online() ? ui::tr(U"CONNECTED") : connection ? ui::tr(U"CONNECTING") : ui::tr(U"未接続");
    p.font(status).draw(12, Arg::topRight = Vec2{1496, 39}, mode == Mode::Live && !online() ? orange : muted);
    Line{254, 83, 1499, 83}.draw(1, line);
    // Status bar
    RectF{0, 976, 1524, 24}.draw(background);
    Line{0, 976, 1524, 976}.draw(1, line);
    Circle{20, 988, 3}.draw(online() ? mint : muted);
    String detail = connection ? connection_status(connection->status()) : mode == Mode::Demo ? ui::tr(U"デモデータ") : mode == Mode::Record ? ui::tr(U"ローカル棋譜") : ui::tr(U"未接続");
    p.clipped(detail, 11, {32, 979, 770, 18}, muted);
    p.label(ui::tr(U"← → 履歴   Space 再生   F11 全画面   F12 スクリーンショット"), 11, 1034, 979, muted);
}

inline void Application::draw_board(Match& m, int sub, RectF region) {
    using namespace ui;
    auto& g = m.game[sub];
    p.card(region);
    Position position = g.pos;
    int shown_ply = g.ply, last = g.last_cell;
    if (cursor() >= 0 && g.history_ok && !g.history.empty()) {
        shown_ply = cursor_for(sub); position = g.history[shown_ply];
        last = shown_ply > 0 ? g.moves[shown_ply - 1].cell : -1;
    }
    p.clipped(ui::tr(U"盤 ") + Format(sub + 1) + U"  /  " + str(g.id), 12, {region.x + 20, region.y + 12, region.w - 188, 21}, sub == 0 ? mint : blue, true);
    String state = !g.active ? ui::tr(U"盤面受信待ち") : pending_moves.count(g.id) ? ui::tr(U"着手送信済み") : cursor() >= 0 ? ui::tr(U"履歴 / ") + Format(shown_ply) : g.finished ? ui::tr(U"終局") : g.pos.to_move == BLACK ? ui::tr(U"黒の手番") : ui::tr(U"白の手番");
    p.clipped(state, 12, {region.rightX() - 155, region.y + 12, 135, 21}, g.finished ? muted : mint);
    Line{region.x + 20, region.y + 36, region.rightX() - 20, region.y + 36}.draw(1, line);
    const double column = (region.w - 60) / 2;
    for (int c = 0; c < 2; ++c) {
        double x = region.x + 20 + c * (column + 20), y = region.y + 45;
        Circle{x + 8, y + 14, 7}.draw(c == BLACK ? ColorF{0.02} : ColorF{0.92}).drawFrame(1, c == BLACK ? line : ColorF{0.95});
        const auto name = g.name[c].empty() ? m.player[c ^ sub] : g.name[c];
        p.clipped(str(name), 16, {x + 24, y, column - 24, 27}, text, true);
        const auto value = g.evaluation(c, shown_ply);
        p.label(ui::tr(U"評価値"), 11, x + 24, y + 29, muted);
        const double eval_x = x + 24 + p.font(ui::tr(U"評価値")).region(11).w + 10;
        p.mono(value ? signed_value(*value) : U"—").draw(18, Vec2{eval_x, y + 25}, value ? text : muted);
        bool running = m.clock_running(sub) && (online() || mode == Mode::Demo);
        p.mono(cursor() >= 0 || !g.has_clock ? U"--:--" : clock_text(g.remaining(c, now(), running))).draw(20, Vec2{x + 24, y + 53}, g.remaining(c, now(), running) < 30 && g.has_clock ? red : text);
        std::optional<double> normal_rating, random_rating;
        if (const auto* user = model.find_user(name)) { normal_rating = user->rating; random_rating = user->random_rating; }
        auto& current_rating = m.type.find_first_of("rR") == std::string::npos ? normal_rating : random_rating;
        if (!current_rating && g.rating[c]) current_rating = g.rating[c];
        p.label(U"8  " + (normal_rating ? ToString(*normal_rating, 0) : U"—"), 10, x + 111, y + 53, muted);
        p.label(U"8r " + (random_rating ? ToString(*random_rating, 0) : U"—"), 10, x + 111, y + 68, muted);
        if (g.active && !g.finished && cursor() < 0 && g.pos.to_move == c && m.clock_running(sub)) RectF{x, region.y + 127, column, 2}.draw(mint);
    }
    double size = Min(region.h - 190, region.w - 80);
    RectF board{region.center().x - size / 2, region.y + 154, size, size};
    const double cell = size / 8;
    board.draw(ColorF{0.073, 0.225, 0.189}).drawFrame(1, ColorF{0.19, 0.37, 0.32});
    uint64_t legal = legal_hints && g.active && !g.finished ? legal_moves(position) : 0;
    const bool can_play = model.may_play(m, sub, online(), cursor()) && !pending_moves.count(g.id) && m.clock_running(sub);
    for (int j = 0; j <= 8; ++j) {
        Line{board.x + j * cell, board.y, board.x + j * cell, board.bottomY()}.draw(1, ColorF{0.19, 0.37, 0.32, 0.7});
        Line{board.x, board.y + j * cell, board.rightX(), board.y + j * cell}.draw(1, ColorF{0.19, 0.37, 0.32, 0.7});
    }
    for (int i = 0; i < 8; ++i) {
        int coord = flip_board ? 7 - i : i;
        p.font(String(1, char32(U'A' + coord))).drawAt(11, {board.x + (i + 0.5) * cell, board.y - 12}, muted);
        p.font(Format(coord + 1)).drawAt(11, {board.x - 21, board.y + (i + 0.5) * cell}, muted);
    }
    for (int i = 0; i < 64; ++i) {
        const int display = flip_board ? 63 - i : i;
        RectF square{board.x + (display % 8) * cell, board.y + (display / 8) * cell, cell, cell};
        Vec2 center = square.center(); int color = at(position, i);
        if (g.active && color >= 0) {
            double rx = cell * 0.39;
            if (animations && cursor() < 0 && now() >= g.changed_ms && now() - g.changed_ms < 330 && at(g.previous, i) >= 0 && at(g.previous, i) != color) {
                double progress = double(now() - g.changed_ms) / 330;
                rx *= Max(0.08, Abs(std::cos(progress * Math::Pi)));
                if (progress < 0.5) color ^= 1;
            }
            Ellipse{center.movedBy(1, 2), rx + 1, cell * 0.4}.draw(ColorF{0.01, 0.06, 0.045, 0.6});
            Ellipse{center, rx, cell * 0.39}.draw(color == BLACK ? ColorF{0.035, 0.055, 0.065} : ColorF{0.91, 0.94, 0.91});
            Ellipse{center.movedBy(-rx * 0.17, -cell * 0.06), rx * 0.82, cell * 0.30}.draw(color == BLACK ? ColorF{0.068, 0.093, 0.105} : ColorF{0.98, 1.0, 0.98});
            if (i == last) Circle{center, 3}.draw(color == BLACK ? mint : ColorF{0.03, 0.35, 0.25});
        } else if (g.active && (legal & ggs::cell_bit(i))) {
            Circle{center, cell * 0.09}.draw(ColorF{mint, can_play ? 0.6 : 0.24});
        }
        if (can_play && flips(position, i) && !p.blocked && square.mouseOver()) {
            square.draw(ColorF{mint, 0.10}); Circle{center, cell * 0.39}.drawFrame(1.5, ColorF{mint, 0.7}); Cursor::RequestStyle(CursorStyle::Hand);
            if (square.leftClicked()) { active_board = sub; play_cell(m, sub, i); }
        }
    }
    if (!g.active) p.font(ui::tr(U"盤面受信待ち")).drawAt(18, board.center(), text);
    double bottom = region.bottomY() - 28;
    Circle{region.x + 26, bottom + 12, 6}.draw(ColorF{0.025}).drawFrame(1, muted);
    p.label(Format(disc_count(position, BLACK)), 15, region.x + 39, bottom, text);
    Circle{region.x + 79, bottom + 12, 6}.draw(ColorF{0.94});
    p.label(Format(disc_count(position, WHITE)), 15, region.x + 92, bottom, text);
    p.label(ui::tr(U"空き ") + Format(64 - std::popcount(position.black | position.white)), 11, region.rightX() - 75, bottom + 3, muted);
    if (can_play && !legal_moves(g.pos)) if (p.button({region.center().x - 51, bottom - 4, 102, 29}, ui::tr(U"パス"), true)) play_cell(m, sub, -1);
}

inline void Application::draw_graph(Match& m, RectF region) {
    using namespace ui;
    p.card(region);
    p.label(ui::tr(U"評価値"), 18, region.x + 18, region.y + 12, text, true);
    if (m.games() == 2) {
        if (p.button({region.x + 128, region.y + 12, 72, 30}, ui::tr(U"2局合算"), graph_board < 0)) graph_board = -1;
        if (p.button({region.x + 206, region.y + 12, 60, 30}, ui::tr(U"盤1"), graph_board == 0)) graph_board = 0;
        if (p.button({region.x + 272, region.y + 12, 60, 30}, ui::tr(U"盤2"), graph_board == 1)) graph_board = 1;
    }
    p.label(ui::tr(U"空きマス数"), 10, region.x + 18, region.y + 44, muted);
    Circle{region.x + 345, region.y + 25, 3}.draw(mint); p.clipped(str(m.player[0]), 12, {region.x + 356, region.y + 15, 100, 26}, mint);
    Circle{region.x + 470, region.y + 25, 3}.draw(blue); p.clipped(str(m.player[1]), 12, {region.x + 481, region.y + 15, 100, 26}, blue);
    if (p.button({region.rightX() - 280, region.y + 12, 36, 30}, U"|‹")) seek(0);
    if (p.button({region.rightX() - 238, region.y + 12, 36, 30}, U"‹")) step_replay(-1);
    if (p.button({region.rightX() - 195, region.y + 12, 54, 30}, playback ? U"Ⅱ" : U"▶")) { if (cursor() < 0) seek(0); playback = !playback; last_playback = now(); }
    if (p.button({region.rightX() - 135, region.y + 12, 36, 30}, U"›")) step_replay(1);
    if (p.button({region.rightX() - 91, region.y + 12, 73, 30}, cursor() < 0 ? mode == Mode::Record ? ui::tr(U"LATEST") : ui::tr(U"LIVE") : ui::tr(U"最新へ"), cursor() < 0)) latest();
    RectF plot{region.x + 47, region.y + 68, region.w - 196, region.h - 102};
    const int sub = m.games() == 2 ? graph_board : 0;
    const auto a = m.graph_series(0, sub), b = m.graph_series(1, sub);
    const double range = m.graph_range_for(sub);
    const int limit = m.x_max(sub);
    auto x_for = [&](int count) { return plot.x + plot.w * Clamp(count, 0, limit) / Max(1, limit); };
    auto y_for = [&](double value) { return plot.center().y - plot.h * 0.5 * value / range; };
    RectF{plot.rightX() + 13, plot.y, 3, plot.h / 2 - 5}.draw(mint);
    RectF{plot.rightX() + 13, plot.center().y + 5, 3, plot.h / 2 - 5}.draw(blue);
    p.label(U"↑", 22, plot.rightX() + 25, plot.y + 3, mint, true);
    p.clipped(str(m.player[0]), 12, {plot.rightX() + 46, plot.y + 4, 96, 22}, mint, true);
    p.label(ui::tr(U"有利"), 10, plot.rightX() + 46, plot.y + 27, muted);
    p.label(U"↓", 22, plot.rightX() + 25, plot.bottomY() - 44, blue, true);
    p.clipped(str(m.player[1]), 12, {plot.rightX() + 46, plot.bottomY() - 43, 96, 22}, blue, true);
    p.label(ui::tr(U"有利"), 10, plot.rightX() + 46, plot.bottomY() - 20, muted);
    int split = graph_board < 0 ? m.graph_divergence() : -1;
    if (split >= 0) {
        double x = x_for(split);
        Line{x, plot.y - 7, x, plot.bottomY()}.draw(1, ColorF{orange, 0.6});
        p.label(ui::tr(U"分岐 / 空き ") + Format(m.empty_squares(split, sub)), 10, Clamp(x + 4, plot.x + 124, plot.rightX() - 96), plot.y - 20, orange);
    }
    for (int j = -2; j <= 2; ++j) {
        double y = y_for(range * j / 2);
        Line{plot.x, y, plot.rightX(), y}.draw(j == 0 ? 1.2 : 1.0, j == 0 ? line : ColorF{line, 0.5});
        p.mono((j > 0 ? U"+" : U"") + ToString(range * j / 2, 0)).draw(10, Vec2{region.x + 10, y - 7}, j > 0 ? mint : j < 0 ? blue : muted);
    }
    const int tick_step = limit <= 30 ? 5 : 10;
    for (int count = 0; count < limit; count += tick_step) {
        if (count > 0 && limit - count < tick_step / 2) continue;
        double x = x_for(count); Line{x, plot.y, x, plot.bottomY()}.draw(1, ColorF{line, 0.4});
        p.font(Format(m.empty_squares(count, sub))).drawAt(10, {x, plot.bottomY() + 15}, muted);
    }
    Line{plot.rightX(), plot.y, plot.rightX(), plot.bottomY()}.draw(1, ColorF{line, 0.6});
    p.font(U"0").drawAt(10, {plot.rightX(), plot.bottomY() + 15}, text);
    auto draw_series = [&](const std::vector<std::pair<int, double>>& values, ColorF color, double sign) {
        LineString points;
        for (const auto& [ply, value] : values) points.push_back({x_for(ply), y_for(value * sign)});
        if (points.size() > 1) points.draw(2, color);
        if (!points.empty()) Circle{points.back(), 3.5}.draw(color);
    };
    draw_series(a, mint, 1); draw_series(b, blue, -1);
    if (a.empty() && b.empty()) p.font(ui::tr(U"評価値未受信")).drawAt(14, plot.center(), muted);
    const bool hovering = !p.blocked && plot.mouseOver();
    if (hovering && MouseL.down()) graph_dragging = true;
    if (!p.blocked && (hovering || graph_dragging)) {
        const int requested_count = Clamp(int(std::lround((Cursor::PosF().x - plot.x) / plot.w * limit)), 0, limit);
        const int hover_count = Min(requested_count, m.max_placements(sub));
        double x = x_for(hover_count); Line{x, plot.y, x, plot.bottomY()}.draw(1, ColorF{text, 0.4});
        String hint = ui::tr(U"空き ") + Format(m.empty_squares(hover_count, sub));
        auto value_at = [&](const auto& values, double sign) { for (auto it = values.rbegin(); it != values.rend(); ++it) if (it->first <= hover_count) return signed_value(it->second * sign); return String{U"—"}; };
        hint += U"   " + value_at(a, 1) + U" / " + value_at(b, -1);
        p.tag(hint, {Clamp(x - 90, plot.x, plot.rightX() - 210), plot.y + 5}, text);
        Cursor::RequestStyle(CursorStyle::Hand);
        if (graph_dragging && MouseL.pressed()) drag_graph(requested_count);
    }
    const double bar_x = x_for(cursor() >= 0 ? graph_cursor(m, sub) : m.max_placements(sub));
    Line{bar_x, plot.y, bar_x, plot.bottomY()}.draw(cursor() >= 0 ? 2 : 1, cursor() >= 0 ? text : ColorF{mint, 0.65});
}

inline void Application::draw_arena() {
    using namespace ui;
    auto* m = selected();
    if (!m) {
        p.card({332, 143, 1240, 810}); p.logo({952, 365}, 1.8);
        p.bold(ui::tr(U"対局未選択")).drawAt(24, {952, 456}, text);
        if (p.button({763, 580, 180, 44}, ui::tr(U"ロビーを開く"), true)) page = 1;
        if (p.button({961, 580, 180, 44}, ui::tr(U"棋譜を開く"))) open_record();
        return;
    }
    p.label(str(m->id) + U"   /   " + str(m->type), 13, 333, 132, muted);
    bool own = may_match() && !m->finished && (m->player[0] == model.username || m->player[1] == model.username);
    std::string game_id = m->game[active_board].id;
    if (own) {
        if (m->games() == 1 && p.button({522, 126, 73, 29}, ui::tr(U"待った"))) ask_confirmation(ui::tr(U"待ったを申し込む"), ui::tr(U"相手に、選択中の盤の待ったを申し込みます。"), "t /os undo " + game_id);
        if (p.button({603, 126, 73, 29}, ui::tr(U"中断"))) ask_confirmation(ui::tr(U"対局の中断を申し込む"), ui::tr(U"相手に対局の中断を申し込みます。"), "t /os abort " + game_id);
        if (p.button({684, 126, 73, 29}, ui::tr(U"投了"), false, true, red)) ask_confirmation(ui::tr(U"選択中の盤を投了する"), ui::tr(U"投了をGGSに送信します。この操作は取り消せません。"), "t /os resign " + game_id);
        if (p.button({765, 126, 73, 29}, ui::tr(U"スコア"))) { modal = "score"; p.focus = "score"; }
    }
    if (model.can_stop_watching(m->id) && p.button({846, 126, 99, 29}, ui::tr(U"観戦を終了"))) { stop_watching(m->id); return; }
    p.tag(m->games() == 2 ? ui::tr(U"SYNCHRO / 2 BOARDS") : ui::tr(U"OTHELLO / 8×8"), {1050, 130}, m->games() == 2 ? mint : blue);
    p.card({332, 166, 942, 68});
    const auto winner = m->winner();
    for (int i = 0; i < 2; ++i) {
        const double x = i == 0 ? 351 : 895;
        RectF{x, 181, 3, 36}.draw(i == 0 ? mint : blue);
        p.clipped(str(m->player[i]), 23, {x + 13, 176, 244, 36}, winner && *winner == i ? mint : text, true);
        if (winner) p.label(*winner < 0 ? ui::tr(U"引き分け") : *winner == i ? ui::tr(U"勝者") : ui::tr(U"敗者"), 13, x + 13, 209, *winner == i ? mint : *winner < 0 ? blue : muted, true);
        else if (m->finished) p.label(ui::tr(U"結果なし"), 12, x + 13, 209, muted);
    }
    p.bold(Format(shown_discs(*m, 0))).drawAt(29, {729, 197}, mint);
    p.bold(Format(shown_discs(*m, 1))).drawAt(29, {850, 197}, blue);
    p.font(ui::tr(U"合計")).drawAt(9, {790, 193}, muted);
    if (auto score = m->result_sum()) p.font(ui::tr(U"石差 ") + signed_value(*score)).drawAt(10, {790, 212}, orange);
    else p.font(m->games() == 2 ? ui::tr(U"2局の石数") : ui::tr(U"石数")).drawAt(9, {790, 211}, muted);
    if (m->games() == 2) { draw_board(*m, 0, {332, 250, 462, 482}); draw_board(*m, 1, {812, 250, 462, 482}); }
    else draw_board(*m, 0, {332, 250, 942, 482});
    draw_graph(*m, {332, 752, 942, 208});
    draw_inspector(*m);
}

inline void Application::draw_lobby() {
    using namespace ui;
    p.label(U"GGS /os", 14, 334, 142, mint);
    p.label(mode == Mode::Demo ? ui::tr(U"サンプルの対局一覧を表示しています") : ui::tr(U"30秒ごとに更新 · 対局を選んで観戦"), 13, 334, 166, muted);
    p.input("filter", filter, {1134, 138, 235, 43}, ui::tr(U"プレイヤー・IDで検索"));
    if (p.button({1384, 138, 78, 43}, ui::tr(U"更新"))) refresh();
    if (may_match() && p.button({1474, 138, 100, 43}, ui::tr(U"申込"), true)) challenge();
    const String tabs[] = {ui::tr(U"対局中"), ui::tr(U"対局募集"), ui::tr(U"観戦・終了局")};
    for (int i = 0; i < 3; ++i) if (p.button({332 + i * 158.0, 212, 144, 40}, tabs[i], lobby_tab == i)) { lobby_tab = i; list_offset = 0; }
    p.card({332, 270, 1242, 690});
    if (lobby_tab == 1) {
        p.label(U"ID", 12, 355, 289, muted); p.label(ui::tr(U"募集者"), 12, 435, 289, muted);
        p.label(ui::tr(U"対局条件"), 12, 635, 289, muted); p.label(ui::tr(U"持ち時間"), 12, 856, 289, muted); p.label(ui::tr(U"相手"), 12, 1114, 289, muted);
        std::vector<Request> requests;
        for (const auto& [id, r] : model.requests) if (filter.value.empty() || str(r.from + r.to + id).lowercased().contains(filter.value.lowercased())) requests.push_back(r);
        p.scroll_list("requests", {332, 320, 1216, 615}, {1558, 326, 10, 576}, int(requests.size()), 9, list_offset);
        for (int i = list_offset; i < int(requests.size()) && i < list_offset + 9; ++i) {
            const auto& r = requests[i]; double y = 326 + (i - list_offset) * 64;
            Line{350, y + 56, 1555, y + 56}.draw(1, ColorF{line, 0.6});
            p.label(str(r.id), 14, 355, y + 14, muted);
            p.clipped(str(r.from), 18, {435, y + 10, 182, 36}, text, true);
            p.label(str(r.type) + (r.flag == "R" ? ui::tr(U"   Rated") : ui::tr(U"   Unrated")), 15, 635, y + 15, mint);
            p.clipped(str(r.clock), 14, {856, y + 15, 224, 30}, muted);
            p.clipped(r.to == "*" ? ui::tr(U"誰でも") : str(r.to), 14, {1114, y + 15, 132, 32});
            const bool mine = r.from == model.username;
            const bool may_accept = may_match() && !mine && (r.to == "*" || r.to == model.username);
            if (may_match() && p.button({1280, y + 10, 121, 36}, mine ? ui::tr(U"取り消す") : ui::tr(U"受ける"), !mine, mine || may_accept)) {
                ask_confirmation(mine ? ui::tr(U"対局申込を取り消す") : ui::tr(U"対局申込を受ける"), str(r.from) + U" / " + str(r.type) + U" / " + str(r.clock), "t /os " + std::string(mine ? "cancel " : "accept ") + r.id);
            }
            if (may_match() && p.button({1416, y + 10, 121, 36}, ui::tr(U"断る"), false, may_accept && r.to != "*")) send("t /os decline " + r.id);
        }
        if (requests.empty()) p.font(ui::tr(U"対局募集なし")).drawAt(19, {951, 558}, muted);
    } else {
        p.label(U"ID", 12, 355, 289, muted); p.label(ui::tr(U"プレイヤー"), 12, 442, 289, muted);
        p.label(ui::tr(U"タイプ"), 12, 866, 289, muted); p.label(ui::tr(U"モード"), 12, 1018, 289, muted); p.label(ui::tr(U"状態"), 12, 1172, 289, muted);
        std::vector<std::string> ids;
        for (const auto& [id, m] : model.matches) {
            bool show = lobby_tab == 0 ? m.listed && !m.finished : m.watching || m.finished;
            if (show && (filter.value.empty() || str(id + m.player[0] + m.player[1]).lowercased().contains(filter.value.lowercased()))) ids.push_back(id);
        }
        p.scroll_list("matches", {332, 320, 1216, 615}, {1558, 326, 10, 592}, int(ids.size()), 8, list_offset);
        for (int i = list_offset; i < int(ids.size()) && i < list_offset + 8; ++i) {
            auto& m = model.matches[ids[i]]; double y = 326 + (i - list_offset) * 74;
            RectF row{346, y, 1210, 66};
            if (row.mouseOver()) Line{row.x, row.bottomY(), row.rightX(), row.bottomY()}.draw(1, muted);
            const auto winner = m.winner();
            p.label(str(m.id), 14, 355, y + 20, muted);
            p.clipped(str(m.player[0]), 18, {442, y + 16, 186, 34}, winner && *winner == 0 ? mint : text, true);
            p.label(U"vs", 13, 640, y + 20, muted);
            p.clipped(str(m.player[1]), 18, {680, y + 16, 170, 34}, winner && *winner == 1 ? mint : text, true);
            p.tag(str(m.type), {866, y + 18}, m.games() == 2 ? mint : blue);
            p.label(m.flag == "R" ? ui::tr(U"Rated") : m.flag == "T" ? ui::tr(U"Tournament") : ui::tr(U"Unrated"), 14, 1018, y + 20, muted);
            p.clipped(winner ? (*winner < 0 ? ui::tr(U"引き分け") : ui::tr(U"勝者 ") + str(m.player[*winner])) : m.finished ? ui::tr(U"結果なし") : ui::tr(U"対局中"), 13, {1172, y + 20, 182, 28}, winner ? mint : muted, bool(winner));
            if (p.button({1370, y + 12, 164, 40}, m.watching ? ui::tr(U"対局を開く →") : ui::tr(U"観戦する →"), !m.watching, mode == Mode::Demo || online() || m.watching)) {
                if (m.watching) { choose_match(m.id); page = 0; } else watch(m.id);
            }
        }
        if (ids.empty()) p.font(ui::tr(U"対局なし")).drawAt(19, {951, 558}, muted);
    }
}

inline void Application::draw_users() {
    using namespace ui;
    p.label(ui::tr(U"ONLINE PLAYERS"), 13, 334, 147, mint);
    p.label(Format(model.users.size()) + ui::tr(U" players · 通常 (8) のレーティング順"), 14, 334, 175, muted);
    p.input("filter", filter, {1175, 145, 272, 43}, ui::tr(U"プレイヤー名を検索"));
    if (p.button({1461, 145, 113, 43}, ui::tr(U"更新"))) refresh_users();
    p.card({332, 215, 1242, 745});
    p.label(ui::tr(U"プレイヤー"), 12, 362, 237, muted); p.label(ui::tr(U"通常 (8)"), 12, 564, 237, muted);
    p.label(ui::tr(U"ランダム (8r)"), 12, 665, 237, muted); p.label(ui::tr(U"状態"), 12, 771, 237, muted);
    std::vector<User> users;
    for (const auto& u : model.users) if (filter.value.empty() || str(u.name).lowercased().contains(filter.value.lowercased())) users.push_back(u);
    p.scroll_list("players", {350, 280, 1195, 650}, {1558, 283, 10, 612}, int(users.size()), 9, list_offset);
    for (int i = list_offset; i < int(users.size()) && i < list_offset + 9; ++i) {
        const auto& u = users[i]; double y = 283 + (i - list_offset) * 68;
        p.clipped(str(u.name), 20, {362, y + 5, 186, 35}, text, true);
        p.mono(u.rating ? ToString(*u.rating, 1) : U"—").draw(19, Vec2{564, y + 7}, text);
        p.mono(u.random_rating ? ToString(*u.random_rating, 1) : U"—").draw(19, Vec2{665, y + 7}, text);
        p.clipped(u.state == "+" ? ui::tr(U"対局募集中") : u.state == "x" ? ui::tr(U"離席") : ui::tr(U"受付停止"), 12, {771, y + 12, 145, 24}, u.state == "+" ? mint : muted);
        if (p.button({928, y, 74, 39}, ui::tr(U"チャット"))) open_chat(u.name);
        if (p.button({1012, y, 101, 39}, ui::tr(U"記録・成績"))) select_player(u.name);
        if (p.button({1123, y, 113, 39}, ui::tr(U"プロフィール"))) show_player_info(u.name);
        if (p.button({1246, y, 100, 39}, ui::tr(U"対局成績"))) show_player_info(u.name, true);
        if (may_match() && p.button({1356, y, 189, 39}, ui::tr(U"対局を申し込む"), true, u.name != model.username && u.state != "x")) challenge(str(u.name));
        Line{362, y + 54, 1547, y + 54}.draw(1, line);
    }
    if (users.empty()) p.font(ui::tr(U"プレイヤーなし")).drawAt(19, {951, 557}, muted);
}

inline void Application::draw_settings() {
    using namespace ui;
    p.card({332, 143, 601, 604}); p.label(ui::tr(U"接続アカウント"), 23, 359, 166, text, true);
    p.logo({403, 260}, 1.20); p.label(ui::tr(U"選択中のアカウント"), 13, 453, 226, muted);
    p.clipped(account_label.value.empty() ? ui::tr(U"新しい接続") : account_label.value, 24, {453, 253, 445, 38}, text, true);
    p.label(U"ID", 12, 360, 329, muted); p.clipped(username.value.empty() ? ui::tr(U"未選択") : username.value, 20, {414, 319, 485, 35}, text, true);
    p.label(ui::tr(U"接続先"), 12, 360, 384, muted); p.clipped(host.value + U":" + port.value, 18, {435, 376, 464, 31}, muted);
    if (p.button({360, 450, 544, 47}, ui::tr(U"接続・アカウント管理を開く →"), true)) { modal = "connect"; p.focus.clear(); }
    if (p.button({360, 516, 544, 42}, ui::tr(U"GGSから切断する"), false, bool(connection))) { cancel_connection(); notify(ui::tr(U"GGSから切断しました")); }
    p.label(Format(accounts.accounts().size()) + ui::tr(U"件のアカウントをローカル保存"), 14, 360, 599, mint);
    p.label(ui::tr(U"IDとパスワードはWindows DPAPIで暗号化します。"), 13, 360, 635, muted);
    p.clipped(connection ? connection_status(connection->status()) : ui::tr(U"未接続"), 13, {360, 703, 544, 32}, online() ? mint : muted);
    p.card({958, 143, 616, 650}); p.label(ui::tr(U"表示・対局設定"), 23, 985, 166, text, true);
    p.label(ui::tr(U"言語"), 18, 985, 225, text, true);
    draw_language_picker({985, 262, 561, 40});
    if (p.toggle({985, 335, 561, 44}, ui::tr(U"合法手のヒントを表示"), legal_hints)) save_settings();
    if (p.toggle({985, 392, 561, 44}, ui::tr(U"石を置く音"), sound)) save_settings();
    if (p.toggle({985, 449, 561, 44}, ui::tr(U"石が返るアニメーション"), animations)) save_settings();
    if (may_match()) {
        if (p.toggle({985, 506, 561, 44}, ui::tr(U"対局申込を受け付ける"), open_for_matches)) send(std::string("t /os open ") + (open_for_matches ? "1" : "0"));
        if (p.toggle({985, 563, 561, 44}, ui::tr(U"レーティング対局"), rated)) send(std::string("t /os rated ") + (rated ? "+" : "-"));
    }
    if (p.button({985, may_match() ? 650.0 : 526.0, 561, 44}, ui::tr(U"棋譜を開く"))) open_record();

}

inline void Application::draw_connection_form(bool startup) {
    using namespace ui;
    const Transformer2D centered{Mat3x2::Translate(-38, 0), TransformCursor::Yes};
    p.card({355, 130, 890, 740}, 20); p.logo({402, 179}, 0.8);
    p.label(startup ? U"GGSB" : ui::tr(U"GGSに接続"), 26, 440, 150, text, true);
    if (startup) {
        p.label(ui::tr(U"GGSに接続"), 13, 440, 187, muted);
        p.tag(ui::tr(U"未接続"), {1110, 158}, muted);
    }
    draw_language_picker({706, 183, 340, 30});
    const Transformer2D compact{Mat3x2::Translate(0, -30), TransformCursor::Yes};
    Line{674, 244, 674, 857}.draw(1, line);
    draw_account_picker({387, 248, 263, 595});
    p.label(ui::tr(U"保存名"), 13, 706, 249, muted); p.input("account-label", account_label, {706, 275, 505, 42}, ui::tr(U"メイン・観戦用など（省略可）"));
    p.label(ui::tr(U"ホスト"), 13, 706, 337, muted); p.label(ui::tr(U"ポート"), 13, 1085, 337, muted);
    p.input("host", host, {706, 363, 355, 42}); p.input("port", port, {1080, 363, 131, 42});
    p.label(ui::tr(U"ID / ユーザー名"), 13, 706, 423, muted); p.input("username", username, {706, 449, 505, 42}, ui::tr(U"GGSのログイン名"));
    p.label(ui::tr(U"パスワード"), 13, 706, 509, muted); bool enter = p.input("password", password, {706, 535, 505, 42}, ui::tr(U"パスワード"), true);
    p.toggle({706, 596, 505, 39}, ui::tr(U"接続時にID・パスワードを保存"), save_login);
    if (p.button({706, 658, 343, 38}, selected_account.empty() ? ui::tr(U"アカウントを保存") : ui::tr(U"変更を保存"), true, accounts.writable())) save_account();
    if (p.button({1065, 658, 146, 38}, ui::tr(U"削除"), false, accounts.writable() && accounts.find(selected_account), red)) delete_account();
    if (startup) {
        if (connection) {
            p.font(connection_status(connection->status())).draw(13, RectF{706, 719, 505, 40}, orange);
            if (p.button({706, 762, 150, 32}, ui::tr(U"接続を中止"))) cancel_connection();
        }
        if (p.button({706, 813, 150, 47}, ui::tr(U"棋譜を開く"))) open_record();
    } else {
        if (p.button({706, 813, 150, 47}, ui::tr(U"キャンセル"))) modal.clear();
    }
    if (p.button({875, 813, 336, 47}, save_login ? ui::tr(U"保存して接続する") : ui::tr(U"接続する"), true) || enter) connect();
}

inline void Application::draw_modal() {
    using namespace ui;
    if (modal.empty()) return;
    RectF{0, 0, 1524, 976}.draw(ColorF{0.01, 0.025, 0.04, 0.83}); p.blocked = false;
    if (modal == "connect") {
        draw_connection_form(false);
        if (KeyEscape.down()) modal.clear();
        return;
    }
    const Transformer2D centered{Mat3x2::Translate(-38, 0), TransformCursor::Yes};
    if (modal == "challenge") {
        p.card({475, 120, 650, 766}, 20);
        p.label(ui::tr(U"対局申込"), 26, 510, 145, text, true);
        const Transformer2D compact{Mat3x2::Translate(0, -24), TransformCursor::Yes};
        p.label(ui::tr(U"対戦相手（空欄なら公開募集）"), 13, 510, 240, muted); p.input("opponent", opponent, {510, 266, 580, 45}, ui::tr(U"プレイヤー名"));
        p.toggle({510, 337, 580, 42}, ui::tr(U"シンクロ戦 · 同じ開始局面で2局"), synchro);
        if (!synchro) random_start = false;
        p.toggle({510, 393, 580, 42}, ui::tr(U"ランダム開始局面"), random_start);
        if (!synchro) random_start = false;
        p.label(ui::tr(U"開始時の石数"), 13, 511, 448, muted); p.input("random-discs", random_discs, {924, 443, 166, 42}, U"14");
        p.label(ui::tr(U"あなたの時計"), 13, 510, 515, muted); p.label(ui::tr(U"相手の時計"), 13, 819, 515, muted);
        p.input("my-clock", my_clock, {510, 541, 274, 45}); p.input("their-clock", their_clock, {817, 541, 273, 45});
        p.label(ui::tr(U"15:00/00:02/00:00 = 15分 + 1手2秒の加算"), 12, 510, 602, muted);
        if (!synchro && !random_start) {
            const String colors[] = {ui::tr(U"色の指定なし"), ui::tr(U"黒番を希望"), ui::tr(U"白番を希望")};
            for (int i = 0; i < 3; ++i) if (p.button({510 + 199.0 * i, 640, 181, 34}, colors[i], color_preference == i)) color_preference = i;
        } else p.label(ui::tr(U"色を入れ替えた2局を同時にプレイします。"), 14, 510, 647, mint);
        std::string preview = challenge_command();
        p.clipped(preview.empty() ? ui::tr(U"時計または開始石数を確認してください") : str(preview), 12, {510, 712, 580, 37}, preview.empty() ? red : muted);
        if (p.button({510, 811, 178, 48}, ui::tr(U"キャンセル"))) modal.clear();
        if (may_match() && p.button({706, 811, 384, 48}, ui::tr(U"対局を申し込む →"), true, !preview.empty())) {
            send(std::string("t /os rated ") + (rated ? "+" : "-")); if (send(preview)) { modal.clear(); notify(ui::tr(U"対局を申し込みました")); }
        }
        if (!online()) p.label(ui::tr(U"GGSに接続すると対局申込を送信できます。"), 13, 510, 761, orange);
    } else if (modal == "confirm") {
        p.card({535, 331, 530, 320}, 20); p.label(confirmation_title, 25, 570, 358, text, true);
        p.clipped(confirmation_description, 15, {570, 426, 460, 73}, muted);
        if (p.button({570, 555, 220, 47}, ui::tr(U"キャンセル"))) modal.clear();
        if (may_match() && p.button({810, 555, 220, 47}, ui::tr(U"送信する"), true, true, confirmation_command.find("resign") != std::string::npos ? red : mint)) {
            if (send(confirmation_command)) modal.clear();
        }
    } else if (modal == "score") {
        p.card({535, 320, 530, 350}, 20); p.label(ui::tr(U"スコアを提案する"), 25, 570, 347, text, true);
        p.label(ui::tr(U"あなたの視点での石差を入力します。"), 14, 570, 407, muted);
        p.input("score", score_input, {570, 447, 460, 46}, U"0");
        double score = 0; bool valid = ggs::parse_double(score_input.value.toUTF8(), score) && Abs(score) <= 64;
        if (p.button({570, 576, 220, 46}, ui::tr(U"キャンセル"))) modal.clear();
        if (may_match() && p.button({810, 576, 220, 46}, ui::tr(U"提案を送る"), true, valid)) {
            if (auto* m = selected()) if (send("t /os score " + m->game[active_board].id + " " + score_input.value.toUTF8())) modal.clear();
        }
    }
    if (KeyEscape.down()) modal.clear();
}

inline void Application::draw() {
    using namespace ui;
    if (!MouseL.pressed() || page != 0 || !modal.empty()) graph_dragging = false;
    p.begin(); p.blocked = !modal.empty();
    if (page == ConnectionPage) {
        RectF{0, 0, 1524, 1000}.draw(background);
        draw_connection_form(true);
    } else {
        draw_shell();
        const Transformer2D compact{Mat3x2::Translate(-76, -30), TransformCursor::Yes};
        if (page == 0) draw_arena(); else if (page == 1) draw_lobby(); else if (page == 2) draw_users(); else if (page == 3) draw_console(); else if (page == 5) draw_mosaic(); else if (page == 6) draw_records(); else if (page == ChatPage) draw_chat(); else draw_settings();
    }
    draw_modal(); p.end();
    if (Scene::Time() < toast_until) {
        double width = Min(1200.0, p.font(toast).region(16).w + 42);
        RectF{762 - width / 2, 923, width, 40}.draw(background).drawFrame(1, mint);
        p.font(toast).drawAt(16, {762, 942}, ui::text);
    }
    if (modal.empty() && p.focus.empty() && page == 0) {
        if (auto* m = selected()) {
            if (KeyLeft.down()) step_replay(-1);
            if (KeyRight.down()) step_replay(1);
            if (KeyHome.down()) seek(0);
            if (KeyEnd.down()) latest();
            if (KeySpace.down()) { if (cursor() < 0) seek(0); playback = !playback; last_playback = now(); }
        }
    }
    if (KeyF11.down()) Window::SetFullscreen(!Window::GetState().fullscreen);
    if (KeyF12.down()) { FileSystem::CreateDirectories(U"Screenshot/"); ScreenCapture::SaveCurrentFrame(U"Screenshot/" + DateTime::Now().format(U"yyyyMMdd-HHmmss") + U".png"); notify(ui::tr(U"スクリーンショットを保存しました")); }
    if (DragDrop::HasNewFilePaths()) for (const auto& file : DragDrop::GetDroppedFilePaths()) load_record(file.path);
}

inline void Application::draw_inspector(Match& m) {
    using namespace ui;
    p.card({1295, 166, 279, 449});
    p.label(ui::tr(U"棋譜"), 18, 1314, 181, text, true);
    if (m.games() == 2) {
        if (p.button({1414, 179, 64, 28}, ui::tr(U"盤1"), active_board == 0)) { active_board = 0; move_offset = 0; move_follow_tail = true; }
        if (p.button({1486, 179, 64, 28}, ui::tr(U"盤2"), active_board == 1)) { active_board = 1; move_offset = 0; move_follow_tail = true; }
    }
    auto& g = m.game[active_board];
    p.label(ui::tr(U"手数"), 10, 1316, 225, muted); p.label(ui::tr(U"黒 / BLACK"), 10, 1354, 225, muted); p.label(ui::tr(U"白 / WHITE"), 10, 1457, 225, muted);
    const int row_count = int((g.moves.size() + 1) / 2);
    const int visible = 9;
    if (move_follow_tail) move_offset = ListScroll::maximum(row_count, visible);
    p.scroll_list("moves", {1310, 246, 238, 261}, {1558, 246, 10, 261}, row_count, visible, move_offset);
    move_follow_tail = move_offset == ListScroll::maximum(row_count, visible);
    const int start = move_offset;
    if (!g.history_ok) p.label(ui::tr(U"履歴を受信しています…"), 13, 1316, 260, muted);
    for (int row = 0; row < visible && start + row < row_count; ++row) {
        const int pair = start + row;
        double y = 249 + row * 29;
        p.label(Format(pair * 2 + 1), 10, 1316, y + 6, muted);
        for (int col = 0; col < 2; ++col) {
            int k = pair * 2 + col; if (k >= int(g.moves.size())) continue;
            const auto& move = g.moves[k]; RectF cell{1350.0 + col * 103, y, 94, 26};
            if (cursor_for(active_board) == move.ply) Line{cell.x, cell.bottomY(), cell.rightX(), cell.bottomY()}.draw(2, mint);
            p.label(str(coordinate(move.pass ? -1 : move.cell)), 15, cell.x + 4, y + 3, cursor_for(active_board) == move.ply ? mint : text, true);
            if (move.has_eval) p.mono(signed_value(move.eval)).draw(10, Vec2{cell.x + 42, y + 7}, muted);
            if (!p.blocked && cell.leftClicked()) seek(move.ply);
        }
    }
    Line{1314, 527, 1553, 527}.draw(1, line);
    if (p.button({1313, 542, 111, 30}, ui::tr(U"棋譜を保存"))) export_record();
    if (p.button({1435, 542, 119, 30}, flip_board ? ui::tr(U"盤の向きを戻す") : ui::tr(U"盤を反転"))) flip_board = !flip_board;
    p.clipped(cursor() >= 0 ? ui::tr(U"履歴閲覧中 · 着手は送信されません") : m.finished ? ui::tr(U"終局 · グラフから棋譜を再生できます") : ui::tr(U"グラフをクリックして履歴を表示"), 10, {1316, 587, 239, 19}, muted);
    p.card({1295, 632, 279, 328});
    p.label(ui::tr(U"チャット"), 17, 1314, 647, text, true);
    p.tag(U"/os", {1506, 645});
    auto& chat = model.match_chat[m.id];
    int chat_y = 685;
    const int chat_max = ListScroll::maximum(int(chat.size()), 4);
    int chat_top = chat_max - Clamp(chat_offset, 0, chat_max);
    p.scroll_list("match-chat", {1312, 681, 236, 178}, {1558, 685, 10, 172}, int(chat.size()), 4, chat_top, 1);
    chat_offset = chat_max - chat_top;
    int end = Max(0, int(chat.size()) - chat_offset), begin = Max(0, end - 4);
    for (int i = begin; i < end; ++i) {
        p.font(str(chat[i])).draw(12, RectF{1315, double(chat_y), 229, 43}, i == begin ? mint : muted); chat_y += 43;
    }
    p.label(ui::tr(U"宛先: /os（対局）・whisper・ユーザー名"), 10, 1314, 870, muted);
    p.input("chat-target", chat_target, {1313, 894, 88, 43}, ui::tr(U"宛先"));
    bool entered = p.input("chat", chat_input, {1408, 894, 111, 43}, ui::tr(U"メッセージ"));
    if (p.button({1527, 894, 28, 43}, U"↑", true) || entered) submit_chat();
}
}
