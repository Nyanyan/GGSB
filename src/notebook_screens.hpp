#pragma once
#include "application.hpp"

namespace ggsb {
inline void Application::draw_mosaic() {
    using namespace ui;
    std::vector<Match*> matches;
    for (auto& [id, m] : model.matches) if (m.watching || model.watches.count(id)) matches.push_back(&m);
    std::stable_sort(matches.begin(), matches.end(), [](const auto* a, const auto* b) { return a->finished < b->finished; });
    int mosaic_page = mosaic_offset / 4;
    p.scroll_list("mosaic", {332, 205, 1242, 780}, {1583, 215, 10, 770}, (int(matches.size()) + 3) / 4, 1, mosaic_page, 1);
    mosaic_offset = mosaic_page * 4;
    p.label(ui::tr(U"観戦中: ") + Format(matches.size()) + ui::tr(U"対戦"), 13, 334, 150, mint, true);
    if (p.button({1209, 145, 72, 36}, U"←", false, mosaic_offset > 0)) mosaic_offset -= 4;
    p.font(Format(mosaic_offset / 4 + 1) + U" / " + Format(Max(1, (int(matches.size()) + 3) / 4))).drawAt(15, {1326, 163}, text);
    if (p.button({1372, 145, 72, 36}, U"→", false, mosaic_offset + 4 < int(matches.size()))) mosaic_offset += 4;
    if (p.button({1459, 145, 115, 36}, ui::tr(U"対局を追加"))) page = 1;
    for (int slot = 0; slot < 4; ++slot) {
        RectF tile{332 + (slot % 2) * 634.0, 215 + (slot / 2) * 397.0, 608, 373}; p.card(tile);
        if (mosaic_offset + slot >= int(matches.size())) {
            p.font(ui::tr(U"対局なし")).drawAt(14, tile.center(), muted); continue;
        }
        auto& m = *matches[mosaic_offset + slot];
        p.clipped(str(m.player[0]) + U"  ×  " + str(m.player[1]), 20, {tile.x + 20, tile.y + 14, 443, 29}, text, true);
        const auto match_winner = m.winner();
        String outcome = match_winner ? (*match_winner < 0 ? ui::tr(U"引き分け") : ui::tr(U"勝者 ") + str(m.player[*match_winner])) : m.finished ? ui::tr(U"結果なし") : ui::tr(U"対局中");
        p.clipped(str(m.id) + U"  /  " + str(m.type) + U"  /  " + outcome, 12, {tile.x + 21, tile.y + 43, tile.w - 42, 20}, match_winner ? mint : muted, bool(match_winner));
        if (p.button({tile.rightX() - 115, tile.y + 16, 94, 31}, ui::tr(U"詳細 →"))) { choose_match(m.id); page = 0; }
        for (int sub = 0; sub < m.games(); ++sub) {
            auto& g = m.game[sub]; RectF board{tile.x + 39 + sub * 299.0, tile.y + 116, 184, 184};
            for (int c = 0; c < 2; ++c) {
                const double y = tile.y + 66 + c * 22;
                p.clipped((c == BLACK ? U"● " : U"○ ") + str(g.name[c]), 11, {board.x, y, 128, 18}, text, true);
                const auto value = g.evaluation(c, g.ply);
                p.mono(value ? signed_value(*value) : U"—").draw(12, Arg::topRight = Vec2{board.rightX(), y}, text);
                if (g.active && !g.finished && g.pos.to_move == c && m.clock_running(sub)) Line{board.x, y + 19, board.rightX(), y + 19}.draw(1, mint);
            }
            board.draw(ColorF{0.073, 0.225, 0.189}).drawFrame(1, line);
            const double cell_size = board.w / 8;
            for (int k = 1; k < 8; ++k) { Line{board.x + k * cell_size, board.y, board.x + k * cell_size, board.bottomY()}.draw(1, ColorF{0.025, 0.12, 0.11}); Line{board.x, board.y + k * cell_size, board.rightX(), board.y + k * cell_size}.draw(1, ColorF{0.025, 0.12, 0.11}); }
            for (int cell = 0; cell < 64; ++cell) {
                int c = at(g.pos, cell); if (c < 0) continue;
                Vec2 center{board.x + (cell % 8 + 0.5) * cell_size, board.y + (cell / 8 + 0.5) * cell_size};
                Circle{center + Vec2{1, 1.4}, cell_size * 0.40}.draw(ColorF{0.015, 0.07, 0.06, 0.7});
                Circle{center, cell_size * 0.39}.draw(c == BLACK ? ColorF{0.025, 0.035, 0.042} : ColorF{0.88, 0.94, 0.95}).drawFrame(0.7, c == BLACK ? ColorF{0.17, 0.23, 0.26} : ColorF{0.96});
                if (cell == g.last_cell) Circle{center, 2}.draw(c == BLACK ? mint : ColorF{0.12, 0.40, 0.31});
            }
            if (!g.active) p.font(ui::tr(U"受信中")).drawAt(16, board.center(), mint);
            p.label(U"● " + Format(disc_count(g.pos, BLACK)) + U"  ○ " + Format(disc_count(g.pos, WHITE)), 11, board.x, board.bottomY() + 5, text);
            p.label(ui::tr(U"空き ") + Format(64 - std::popcount(g.pos.black | g.pos.white)), 10, board.rightX() - 48, board.bottomY() + 6, muted);
        }
        if (m.games() == 1) {
            p.label(ui::tr(U"BOARD 1"), 11, tile.x + 291, tile.y + 86, mint, true);
            for (int c = 0; c < 2; ++c) {
                const auto& g = m.game[0]; double y = tile.y + 123 + c * 70;
                p.clipped(str(g.name[c]), 21, {tile.x + 291, y, 230, 29}, text, true);
                p.mono(g.has_clock ? clock_text(g.remaining(c, now(), m.clock_running(0) && (online() || mode == Mode::Demo))) : U"--:--").draw(19, Vec2{tile.x + 291, y + 30}, g.pos.to_move == c && !g.finished ? mint : muted);
                p.font(Format(disc_count(g.pos, c))).drawAt(29, {tile.x + 550, y + 29}, text);
            }
        }
        RectF graph{tile.x + 21, tile.y + 337, 533, 20};
        Line{graph.x, graph.center().y, graph.rightX(), graph.center().y}.draw(1, line);
        const auto a = m.graph_series(0), b = m.graph_series(1);
        const double scale = graph_range(a, b);
        for (int idx = 0; idx < 2; ++idx) {
            const auto& data = idx == 0 ? a : b; LineString curve;
            for (const auto& v : data) curve << Vec2{graph.x + graph.w * v.first / Max(1, m.x_max()), graph.center().y - graph.h * 0.46 * v.second * (idx == 0 ? 1 : -1) / scale};
            if (curve.size() >= 2) curve.draw(1.6, idx == 0 ? mint : blue);
        }
        p.label(m.games() == 2 ? ui::tr(U"2局合算") : ui::tr(U"評価値"), 9, graph.x, graph.y - 11, muted);
        p.clipped(U"↑ " + str(m.player[0]), 9, {graph.x + 110, graph.y - 11, 181, 13}, mint);
        p.clipped(U"↓ " + str(m.player[1]), 9, {graph.x + 305, graph.y - 11, 181, 13}, blue);
        p.mono(U"+" + ToString(scale, 0)).draw(8, Vec2{graph.rightX() + 5, graph.y - 3}, mint);
        p.mono(U"−" + ToString(scale, 0)).draw(8, Vec2{graph.rightX() + 5, graph.bottomY() - 9}, blue);
        p.font(Format(m.x_max())).drawAt(8, {graph.x, graph.bottomY() + 6}, muted);
        p.font(ui::tr(U"空きマス数")).drawAt(8, {graph.center().x, graph.bottomY() + 6}, muted);
        p.font(U"0").drawAt(8, {graph.rightX(), graph.bottomY() + 6}, muted);
    }
}

inline void Application::draw_records() {
    using namespace ui;
    auto& book = record_book();
    bool entered = p.input("tracking-player", tracking_player, {332, 143, 225, 42}, ui::tr(U"プレイヤー名"));
    if (p.button({569, 143, 101, 42}, ui::tr(U"選択")) || entered) select_player(tracking_player.value.toUTF8());
    if (p.button({682, 143, 189, 42}, ui::tr(U"この時点から記録"), true, online() || mode == Mode::Demo)) start_tracking();
    auto* session = book.find(selected_session);
    if (p.button({883, 143, 117, 42}, ui::tr(U"記録を停止"), false, session && session->active)) { session->active = false; if (mode != Mode::Demo) save_records(); }
    if (p.button({1012, 143, 162, 42}, ui::tr(U"履歴・集計を保存"))) export_player_records();
    if (p.button({1186, 143, 181, 42}, ui::tr(U"保存フォルダー"))) { FileSystem::CreateDirectories(U"records/"); System::LaunchFile(FileSystem::FullPath(U"records/")); }
    if (p.button({1379, 143, 195, 42}, ui::tr(U"サーバー履歴を取得"), false, online())) {
        const auto name = session ? session->player : tracking_player.value.toUTF8(); if (token_safe(name)) send("t /os history " + name);
    }
    p.card({332, 215, 273, 745}); p.label(ui::tr(U"記録した区間"), 18, 352, 234, text, true);
    p.label(ui::tr(U"新しく開始しても前の区間は残ります"), 11, 352, 264, muted);
    p.scroll_list("sessions", {345, 296, 236, 596}, {589, 300, 10, 576}, int(book.sessions.size()), 8, sessions_offset, 2);
    int n = 0;
    for (auto it = book.sessions.rbegin() + sessions_offset; it != book.sessions.rend() && n < 8; ++it, ++n) {
        auto& s = *it; RectF row{345, 300 + n * 72.0, 235, 63};
        if (s.id == selected_session) RectF{row.x, row.y + 6, 3, row.h - 12}.draw(mint);
        Circle{360, row.y + 15, 3}.draw(s.active ? mint : muted);
        p.clipped(str(s.player), 17, {372, row.y + 4, 201, 24}, text, true);
        p.clipped(str(s.started), 11, {359, row.y + 30, 216, 20}, muted);
        p.label(Format(s.included.size()) + ui::tr(U"対戦  /  ") + (s.active ? ui::tr(U"記録中") : ui::tr(U"停止中")), 10, 359, row.y + 47, s.active ? mint : muted);
        if (!p.blocked && p.dragging_list.empty() && row.leftClicked()) { selected_session = s.id; tracking_player.value = str(s.player); tracking_player.cursor = tracking_player.value.size(); records_offset = 0; }
    }
    if (!n) p.font(ui::tr(U"プレイヤーを選び、記録を開始すると区間がここに残ります。")).draw(15, RectF{355, 322, 224, 120}, muted);
    p.label(mode == Mode::Demo ? ui::tr(U"DEMO / サンプル集計") : ui::tr(U"自動保存 · records/players.ggsb"), 11, 352, 926, mode == Mode::Demo ? orange : muted);
    session = book.find(selected_session);
    const std::string player = session ? session->player : tracking_player.value.toUTF8();
    const std::string scope = session ? session->scope : record_scope();
    auto stats = session ? book.stats(*session) : std::vector<OpponentStats>{};
    OpponentStats total;
    for (const auto& s : stats) { total.wins += s.wins; total.draws += s.draws; total.losses += s.losses; total.boards += s.boards; total.margin_sum += s.margin_sum; total.synchro += s.synchro; }
    const String titles[] = {ui::tr(U"対戦数 / 勝・分・負"), ui::tr(U"勝率 / 引き分け = 0.5勝"), ui::tr(U"平均石差 / 1局")};
    const String values[] = {Format(total.matches()) + U"   " + Format(total.wins) + U"・" + Format(total.draws) + U"・" + Format(total.losses), total.matches() ? ToString(total.win_rate() * 100, 1) + U"%" : U"—", total.boards ? signed_value(total.average_margin(), 2) : U"—"};
    for (int i = 0; i < 3; ++i) { RectF card{626 + i * 321.0, 215, 306, 95}; p.card(card); p.label(titles[i], 12, card.x + 18, card.y + 13, muted); p.label(values[i], 30, card.x + 18, card.y + 43, i == 1 ? mint : text, true); }
    p.clipped(str(player) + U"  /  " + (session ? str(session->started) + ui::tr(U" から  /  ") + str(session->scope) : ui::tr(U"記録はまだ開始していません")), 14, {628, 326, 945, 25}, text, true);
    if (p.button({626, 366, 147, 34}, ui::tr(U"相手別の成績"), records_tab == 0)) { records_tab = 0; records_offset = 0; }
    if (p.button({786, 366, 147, 34}, ui::tr(U"保存した対局履歴"), records_tab == 1)) { records_tab = 1; records_offset = 0; }
    p.clipped(session && session->active ? (session->baseline_ready ? ui::tr(U"● 記録中 · 対局を自動観戦") : ui::tr(U"● 記録中 · 最初の履歴を確認中")) : ui::tr(U"保存済みの区間を閲覧"), 12, {958, 375, 614, 23}, session && session->active ? mint : muted);
    p.card({626, 419, 948, 400});
    std::vector<const RecordedResult*> history;
    for (const auto& [key, r] : book.results) if (r.scope == scope && r.index(player) >= 0) history.push_back(&r);
    std::sort(history.begin(), history.end(), [](const auto* a, const auto* b) { return archive_number(a->archive_id) > archive_number(b->archive_id); });
    const int count = records_tab == 0 ? int(stats.size()) : int(history.size());
    p.scroll_list("records-table", {641, 462, 909, 338}, {1558, 476, 10, 308}, count, 6, records_offset, 2);
    if (records_tab == 0) {
        p.label(ui::tr(U"対戦相手"), 12, 650, 437, muted); p.label(ui::tr(U"対戦 (SYNC)"), 12, 875, 437, muted); p.label(ui::tr(U"勝・分・負"), 12, 1053, 437, muted); p.label(ui::tr(U"勝率"), 12, 1228, 437, muted); p.label(ui::tr(U"平均石差 / 1局"), 12, 1403, 437, muted);
        for (int i = records_offset; i < count && i < records_offset + 6; ++i) {
            const auto& s = stats[i]; double y = 481 + (i - records_offset) * 51;
            p.clipped(str(s.opponent), 18, {650, y, 210, 30}, text, true);
            p.label(Format(s.matches()) + U" (" + Format(s.synchro) + U")", 17, 875, y, muted);
            p.label(Format(s.wins) + U"・" + Format(s.draws) + U"・" + Format(s.losses), 17, 1054, y, text);
            p.label(ToString(s.win_rate() * 100, 1) + U"%", 21, 1228, y - 3, mint, true);
            p.label(signed_value(s.average_margin(), 2), 21, 1403, y - 3, text, true);
            Line{650, y + 35, 1550, y + 35}.draw(1, line);
        }
    } else {
        p.label(ui::tr(U"対局ID / 終局時刻"), 12, 650, 437, muted); p.label(ui::tr(U"対戦相手"), 12, 926, 437, muted); p.label(ui::tr(U"種別"), 12, 1137, 437, muted); p.label(ui::tr(U"結果 / 1局石差"), 12, 1263, 437, muted); p.label(ui::tr(U"棋譜"), 12, 1479, 437, muted);
        for (int i = records_offset; i < count && i < records_offset + 6; ++i) {
            const auto& r = *history[i]; int idx = r.index(player); double score = r.score(idx), y = 476 + (i - records_offset) * 52;
            p.label(str(r.archive_id.empty() ? r.match_id : r.archive_id), 16, 650, y, text, true);
            p.clipped(str(r.ended), 10, {650, y + 23, 267, 16}, muted);
            p.clipped(str(r.player[1 - idx]), 17, {926, y + 6, 199, 25}, text);
            p.label(str(r.type), 13, 1137, y + 8, muted);
            p.label((score > 0 ? ui::tr(U"勝 ") : score < 0 ? ui::tr(U"負 ") : ui::tr(U"分 ")) + signed_value(score), 17, 1263, y + 6, score > 0 ? mint : score < 0 ? red : blue, true);
            if (p.button({1464, y + 2, 90, 32}, r.ggf_path.empty() ? ui::tr(U"未保存") : ui::tr(U"開く →"), false, !r.ggf_path.empty() && FileSystem::Exists(str(r.ggf_path)))) { auto path = str(r.ggf_path); load_record(path); return; }
            Line{650, y + 43, 1550, y + 43}.draw(1, line);
        }
    }
    if (!count) p.font(records_tab == 0 ? ui::tr(U"記録開始後の終局が届くと、相手別の成績が表示されます。") : ui::tr(U"サーバー履歴を取得するか、対局の終局を待ってください。")).drawAt(15, {1100, 607}, muted);
    p.label(Format(count) + ui::tr(U"件  ·  スクロールで続きを表示"), 11, 646, 799, muted);
    p.card({626, 837, 948, 123});
    p.label(ui::tr(U"シンクロ戦は2局合わせて1対戦。勝率は (勝 + 0.5 × 分) ÷ 対戦数。"), 13, 648, 853, text);
    p.label(ui::tr(U"平均石差は自分の視点で計算し、シンクロ戦も1局あたりの値です。"), 12, 648, 883, muted);
    p.clipped(records_status.empty() ? ui::tr(U"接続中に観戦できた棋譜を自動保存。サーバーに残る結果履歴も保存します。") : records_status, 12, {648, 919, 904, 22}, mode == Mode::Demo ? orange : muted);
}
}
