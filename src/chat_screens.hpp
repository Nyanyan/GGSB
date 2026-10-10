#pragma once
#include "application.hpp"

namespace ggsb {
inline void Application::draw_chat() {
    using namespace ui;
    const auto target = model.chats.selected;
    auto& thread = model.chats.ensure(target);
    thread.unread = 0;
    auto& state = chat_views[target];
    const RectF output{352, 234, 1176, 600};
    const int columns = Max(1, int(output.w / p.mono(U"M").region(16).w));
    std::vector<std::u32string> lines;
    for (const auto& [id, count] : state.row_counts) if (id < thread.first_message) state.origin += count;
    state.row_counts.clear();
    uint64_t id = thread.first_message;
    for (const auto& entry : thread.messages) {
        const auto value = str((entry.outgoing ? "> " : "") + entry.sender + ": " + entry.text).toUTF32();
        const int first_row = int(lines.size());
        std::u32string row; int width = 0;
        for (char32_t ch : value) {
            if (ch == U'\r') continue;
            const int advance = ConsoleBuffer::width(ch);
            if (ch == U'\n' || width + advance > columns) { lines.push_back(std::move(row)); row.clear(); width = 0; }
            if (ch != U'\n') { row += ch; width += advance; }
        }
        lines.push_back(std::move(row)); lines.emplace_back();
        state.row_counts.emplace_back(id++, int(lines.size()) - first_row);
    }
    state.output.sync(std::move(lines), state.origin);
    p.label(str(target), 23, 334, 137, text, true);
    p.label(target == ".chat" ? ui::tr(U"全体チャット") : target.starts_with(".") ? ui::tr(U"チャンネル") : target.starts_with("/") ? ui::tr(U"サービス") : ui::tr(U"個人チャット"), 13, 334, 173, muted);
    if (p.button({1126, 141, 139, 36}, ui::tr(U"コピー"), false, state.output.selected())) Clipboard::SetText(String{state.output.selection_text()});
    if (p.button({1279, 141, 140, 36}, ui::tr(U"最新に戻る"))) state.output.scroll_to(state.output.max_top());
    if (p.button({1433, 141, 141, 36}, ui::tr(U"表示をクリア"))) { thread.first_message += thread.messages.size(); thread.messages.clear(); state = {}; }
    p.card({332, 215, 1242, 674});
    draw_text_output(state.output, output, "conversation-output");
    auto& draft = chat_drafts[target];
    const bool entered = p.input("conversation-input", draft, {332, 909, 1120, 48}, ui::tr(U"メッセージ"));
    if (p.button({1466, 909, 108, 48}, ui::tr(U"送信"), true, online() && !draft.value.empty()) || entered) submit_conversation();
}
}
