#pragma once
#include "application.hpp"

namespace ggsb {
inline void Application::sync_console() {
    std::vector<std::u32string> lines;
    for (const auto& message : model.console) {
        lines.push_back(ui::str(message.head).toUTF32());
        for (const auto& body : message.body) lines.push_back(ui::str(body).toUTF32());
    }
    console_view.sync(std::move(lines), model.console_first_line);
}
inline void Application::seed_console_preview() {
    connection.reset(); mode = Mode::Offline; model = {}; console_view = {}; reset_chat_views(); page = 3;
    for (int i = 0; i < 360; ++i) {
        const auto number = std::to_string(i);
        model.append_console({"> t /os history sample-player " + number, {
            "." + number + "   2026-10-09 12:34:56   sample-player   sample-opponent   s8r14   R   +2.00",
            "ASCII alignment: 0123456789  ABCDEFGHIJ  abcdefghij    日本語の選択とコピー    " + std::string(180, '.')
        }});
    }
    sync_console(); console_view.scroll_to(72); console_view.begin_selection({76, 3}); console_view.extend_selection({79, 34}); console_view.selecting = false;
    p.focus = "console-output";
}
inline void Application::draw_text_output(ConsoleBuffer& view, RectF output, const std::string& focus_id) {
    using namespace ui;
    const double font_size = 16, row_height = 24, cell_width = p.mono(U"M").region(font_size).w;
    view.viewport(int(output.h / row_height), int(output.w / cell_width));
    const RectF vertical{output.rightX() + 18, output.y, 12, output.h};
    const RectF horizontal{output.x, output.bottomY() + 18, output.w, 10};
    const double thumb_height = Min(vertical.h, Max(32.0, vertical.h * view.rows / Max(1, int(view.lines.size()))));
    const double thumb_width = Min(horizontal.w, Max(32.0, horizontal.w * view.columns / Max(1, view.widest())));
    RectF vthumb{vertical.x, vertical.y + (vertical.h - thumb_height) * view.top / Max(1, view.max_top()), vertical.w, thumb_height};
    RectF hthumb{horizontal.x + (horizontal.w - thumb_width) * view.left / Max(1, view.max_left()), horizontal.y, thumb_width, horizontal.h};
    const Vec2 mouse = Cursor::PosF();
    if (!MouseL.pressed() || p.blocked) { console_drag_vertical = console_drag_horizontal = false; view.selecting = false; }
    if (!p.blocked) {
        if (output.mouseOver()) {
            Cursor::RequestStyle(CursorStyle::IBeam);
            view.wheel(KeyShift.pressed() ? 0 : Mouse::Wheel(), Mouse::WheelH() + (KeyShift.pressed() ? Mouse::Wheel() : 0), KeyControl.pressed());
            if (MouseL.down()) {
                p.focus = focus_id; p.hit_field = true;
                view.begin_selection(view.hit(view.top + int((mouse.y - output.y) / row_height), view.left + (mouse.x - output.x) / cell_width), KeyShift.pressed());
            }
        }
        if (vertical.mouseOver() && MouseL.down() && view.max_top()) {
            console_drag_vertical = true; console_drag_offset = vthumb.mouseOver() ? mouse.y - vthumb.y : thumb_height / 2;
            p.focus = focus_id; p.hit_field = true;
        }
        if (horizontal.mouseOver() && MouseL.down() && view.max_left()) {
            console_drag_horizontal = true; console_drag_offset = hthumb.mouseOver() ? mouse.x - hthumb.x : thumb_width / 2;
            p.focus = focus_id; p.hit_field = true;
        }
        if (!view.max_top()) console_drag_vertical = false;
        if (!view.max_left()) console_drag_horizontal = false;
        if (console_drag_vertical) view.drag_vertical((mouse.y - vertical.y - console_drag_offset) / (vertical.h - thumb_height));
        if (console_drag_horizontal) view.drag_horizontal((mouse.x - horizontal.x - console_drag_offset) / (horizontal.w - thumb_width));
        if (view.selecting) {
            if (Scene::Time() >= console_autoscroll_due) {
                if (mouse.y < output.y) view.scroll_by(-2); else if (mouse.y >= output.bottomY()) view.scroll_by(2);
                if (mouse.x < output.x) view.left = Max(0, view.left - 3); else if (mouse.x > output.rightX()) view.left = Min(view.max_left(), view.left + 3);
                console_autoscroll_due = Scene::Time() + 0.03;
            }
            view.extend_selection(view.hit(view.top + Clamp(int(std::floor((mouse.y - output.y) / row_height)), 0, view.rows - 1), view.left + (mouse.x - output.x) / cell_width));
        }
        if (p.focus == focus_id) {
            if (KeyPageUp.down()) view.scroll_by(-view.rows);
            if (KeyPageDown.down()) view.scroll_by(view.rows);
            if (KeyHome.down()) view.scroll_to(0);
            if (KeyEnd.down()) view.scroll_to(view.max_top());
            if (KeyUp.down()) view.scroll_by(-1);
            if (KeyDown.down()) view.scroll_by(1);
            if (KeyLeft.down()) view.left = Max(0, view.left - 4);
            if (KeyRight.down()) view.left = Min(view.max_left(), view.left + 4);
            if (KeyControl.pressed() && KeyA.down()) view.select_all();
            if (KeyControl.pressed() && KeyC.down() && view.selected()) Clipboard::SetText(String{view.selection_text()});
        }
    }
    const auto [begin, end] = view.selection();
    for (int row = view.top; row < Min(int(view.lines.size()), view.top + view.rows); ++row) {
        const auto& value = view.lines[row]; const double y = output.y + (row - view.top) * row_height;
        if (view.selected() && row >= begin.row && row <= end.row) {
            const int first = row == begin.row ? view.cell_column(row, begin.column) : 0;
            const int last = row == end.row ? view.cell_column(row, end.column) : view.cell_column(row, int(value.size())) + 1;
            const double x0 = Clamp(output.x + (first - view.left) * cell_width, output.x, output.rightX());
            const double x1 = Clamp(output.x + (last - view.left) * cell_width, output.x, output.rightX());
            if (x1 > x0) RectF{x0, y, x1 - x0, row_height}.draw(ColorF{mint, 0.23});
        }
        const bool sent = value.starts_with(U"> "), error = value.find(U"ERR") != std::u32string::npos;
        const ColorF color = error ? red : sent ? mint : text;
        int cell = 0;
        for (char32_t ch : value) {
            const int advance = ConsoleBuffer::width(ch);
            if (cell >= view.left && cell + advance <= view.left + view.columns && ch != U'\t')
                p.mono(String(1, ch)).draw(font_size, Vec2{output.x + (cell - view.left) * cell_width, y + 2}, color);
            cell += advance; if (cell > view.left + view.columns) break;
        }
    }
    if (view.lines.empty()) p.mono(ui::tr(U"メッセージなし")).draw(16, output.pos, muted);
    vertical.draw(background);
    Line{vertical.center().x, vertical.y, vertical.center().x, vertical.bottomY()}.draw(2, line);
    Line{horizontal.x, horizontal.center().y, horizontal.rightX(), horizontal.center().y}.draw(2, line);
    if (view.max_top()) { vthumb.y = vertical.y + (vertical.h - thumb_height) * view.top / view.max_top(); vthumb.draw(console_drag_vertical ? mint : muted); }
    if (view.max_left()) { horizontal.draw(background); hthumb.x = horizontal.x + (horizontal.w - thumb_width) * view.left / view.max_left(); hthumb.draw(console_drag_horizontal ? mint : muted); }
    p.mono(Format(view.lines.empty() ? 0 : view.top + 1) + U"–" + Format(Min(int(view.lines.size()), view.top + view.rows)) + U" / " + Format(view.lines.size())).draw(11, Vec2{output.x + 1, output.bottomY() + 34}, muted);
    p.label(view.follow_tail ? ui::tr(U"最新を表示") : ui::tr(U"履歴を表示"), 11, output.rightX() - 80, output.bottomY() + 34, view.follow_tail ? mint : muted);
}

inline void Application::draw_console() {
    using namespace ui;
    sync_console();
    p.label(U"/os", 13, 334, 142, mint);
    p.label(ui::tr(U"ドラッグ選択 · Ctrl+C · PgUp/PgDn · Home/End"), 13, 334, 173, muted);
    if (p.button({972, 141, 139, 36}, ui::tr(U"ログフォルダー"))) System::LaunchFile(FileSystem::FullPath(U"logs/"));
    if (p.button({1126, 141, 139, 36}, ui::tr(U"コピー"), false, console_view.selected())) Clipboard::SetText(String{console_view.selection_text()});
    if (p.button({1279, 141, 140, 36}, ui::tr(U"最新に戻る"))) console_view.scroll_to(console_view.max_top());
    if (p.button({1433, 141, 141, 36}, ui::tr(U"表示をクリア"))) { model.clear_console(); console_view = {}; sync_console(); }
    p.card({332, 215, 1242, 674});
    draw_text_output(console_view, {352, 234, 1176, 600}, "console-output");
    bool entered = p.input("command", command, {332, 909, 1120, 48}, U"t /os match", false, true);
    if (p.button({1466, 909, 108, 48}, ui::tr(U"送信"), true, online()) || entered) {
        if (!command.value.empty() && send(command.value.toUTF8())) { command.value.clear(); command.cursor = 0; console_view.scroll_to(console_view.max_top()); }
    }
}
}
