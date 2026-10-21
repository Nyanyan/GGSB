#pragma once
#include "../src/application.hpp"

namespace ggsb {
inline bool verify_text_size_workflow(Application& app, String& result) {
    auto fail = [&](const String& message) { result = U"FAIL: text size / " + message; return false; };
    if (app.p.enlarge_small_text) return fail(U"standard default");
    const auto original_command = app.command;
    const auto original_drafts = app.chat_drafts;
    const auto original_chat = app.model.chats;
    const auto original_console = app.console_view;
    app.command.value = U"partly typed command";
    app.chat_drafts["bob"].value = U"編集中のメッセージ";
    app.model.chats.append("bob", "bob", "received message", false);
    app.console_view.sync({U"abc", U"日本語"}, 0);
    app.console_view.begin_selection({0, 0}); app.console_view.extend_selection({1, 3});
    const bool hints = app.legal_hints;
    const auto theme = ui::theme_name(), language = ui::language_pack().selected();
    for (const String size : {U"large", U"standard"}) {
        app.set_text_size(size);
        INI saved{U"settings.ini"};
        Application reopened;
        if (reopened.p.enlarge_small_text != (size == U"large") || saved.getOr<bool>(U"view.enlarge_small_text", size != U"large") != reopened.p.enlarge_small_text)
            return fail(U"selection restored after restart");
        if (reopened.legal_hints != hints || ui::theme_name() != theme || ui::language_pack().selected() != language)
            return fail(U"other display settings preserved");
        // Player-list status, headings, ratings and names use these sizes.
        for (const double original_size : {11.0, 12.0, 13.0, 14.0}) for (const Font* font : {&app.p.font, &app.p.mono}) {
            const double normal_width = (*font)(U"Player 123").region(original_size).w;
            const double shown_width = (*font)(U"Player 123").region(app.p.font_size(original_size)).w;
            if (Abs(shown_width / normal_width - (size == U"large" ? 1.3 : 1.0)) > 0.001)
                return fail(U"player-list font metrics / 30 percent enlargement");
        }
        for (const double original_size : {16.0, 17.0, 18.0, 23.0, 30.0})
            if (app.p.font_size(original_size) != original_size) return fail(U"body, input and heading sizes unchanged");
        const String button_titles[] = {U"チャット", U"記録・成績", U"プロフィール", U"対局成績"};
        const double button_widths[] = {74, 101, 113, 100};
        for (int i = 0; i < 4; ++i) {
            const double actual_size = app.p.button_font_size({0, 0, button_widths[i], 28}, button_titles[i], 12);
            if (Abs(actual_size - (size == U"large" ? 15.6 : 12.0)) > 0.001)
                return fail(U"player-list button " + button_titles[i] + U" rendered size=" + ToString(actual_size, 3)
                    + U", target bounds=" + Format(app.p.font(button_titles[i]).region(app.p.font_size(12)).size));
        }
        if (app.command.value != U"partly typed command" || app.chat_drafts["bob"].value != U"編集中のメッセージ"
            || app.model.chats.threads.at("bob").messages.back().text != "received message"
            || app.console_view.selection_text() != U"abc\n日本語") return fail(U"drafts, messages and console selection preserved");
    }
    INI invalid{U"settings.ini"}; invalid.write(U"view", U"enlarge_small_text", U"invalid");
    if (!invalid.save(U"settings.ini")) return fail(U"invalid setting fixture");
    { Application reopened; if (reopened.p.enlarge_small_text) return fail(U"invalid setting fallback"); }
    app.set_text_size(U"standard");
    app.command = original_command; app.chat_drafts = original_drafts;
    app.model.chats = original_chat; app.console_view = original_console;
    return true;
}
}
