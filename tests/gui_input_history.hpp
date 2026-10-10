#pragma once
#include "../src/application.hpp"

inline bool verify_input_history(ggsb::Application& app, s3d::String& result) {
    using namespace s3d;
    auto fail = [&](const String& reason) { result = U"FAIL: input history / " + reason; return false; };
    app.command.value = U"t /os help";
    if (!app.submit_command()) return fail(U"first console command");
    app.command.value = U"t /os stored";
    if (!app.submit_command() || app.command.history.size() != 2 || !app.command.value.empty()) return fail(U"console history after sending");
    app.command.value = U"編集中の入力🙂"; app.command.cursor = 3; app.command.all = true;
    if (app.command.recall_history(true, false, true) || app.command.value != U"編集中の入力🙂") return fail(U"IME candidate selection");
    app.command.was_composing = true;
    if (app.command.recall_history(true, false)) return fail(U"IME completion frame");
    app.command.was_composing = false;
    if (!app.command.recall_history(true, false) || app.command.value != U"t /os stored" || app.command.all
        || !app.command.recall_history(true, false) || app.command.value != U"t /os help"
        || app.command.recall_history(true, false)) return fail(U"console older commands");
    if (!app.command.recall_history(false, true) || app.command.value != U"t /os stored"
        || !app.command.recall_history(false, true) || app.command.value != U"編集中の入力🙂"
        || app.command.cursor != 3 || !app.command.all || app.command.recall_history(false, true)) return fail(U"console draft restoration");
    for (const auto& target : {"bob", ".chat", ".tourney", "/os"}) {
        app.open_chat(target);
        auto& draft = app.chat_drafts[target];
        const String text = U"history for " + ggsb::ui::str(target);
        draft.value = text; app.submit_conversation();
        if (draft.history.size() != 1 || !draft.value.empty()) return fail(U"conversation history after sending");
        draft.value = U"chat draft"; draft.cursor = 4;
        if (!draft.recall_history(true, false) || draft.value != text
            || !draft.recall_history(false, true) || draft.value != U"chat draft" || draft.cursor != 4) return fail(U"conversation draft restoration");
    }
    app.close_chat("bob"); app.open_chat("bob");
    if (!app.chat_drafts["bob"].recall_history(true, false) || app.chat_drafts["bob"].value != U"history for bob") return fail(U"reopened conversation history");
    app.model.selected = ".117";
    for (const auto& target : {"/os", "whisper"}) {
        app.chat_target.value = ggsb::ui::str(target);
        auto& draft = app.match_chat_input();
        const String text = U"match history for " + ggsb::ui::str(target);
        draft.value = text; app.submit_chat();
        if (draft.history.size() != 1 || !draft.value.empty() || !draft.recall_history(true, false) || draft.value != text) return fail(U"match recipient history");
    }
    app.model.selected = ".118";
    if (app.match_chat_input().history.size() || app.match_chat_input().recall_history(true, false)) return fail(U"separate match histories");
    app.model.selected = ".117"; app.chat_target.value = U"/os";
    if (app.match_chat_input().value != U"match history for /os") return fail(U"match draft preservation");
    const auto mode = app.mode; app.mode = ggsb::Application::Mode::Offline;
    app.command.value = U"unsent command";
    if (app.submit_command() || app.command.value != U"unsent command" || app.command.history.size() != 2) return fail(U"failed console send");
    app.chat_drafts["bob"].value = U"unsent private message"; app.submit_conversation();
    if (app.chat_drafts["bob"].value != U"unsent private message" || app.chat_drafts["bob"].history.size() != 1) return fail(U"failed conversation send");
    app.match_chat_input().value = U"unsent match message"; app.submit_chat();
    if (app.match_chat_input().value != U"unsent match message" || app.match_chat_input().history.size() != 1) return fail(U"failed match send");
    app.mode = mode; app.toast.clear(); app.page = 3; app.p.focus = "command";
    app.command.value.clear(); app.command.cursor = 0; app.command.all = false;
    if (!app.command.recall_history(true, false) || app.command.value != U"t /os stored") return fail(U"recall after failed send");
    result = U"PASS: console and chat input history, draft restoration, recipient isolation, IME protection, failed sends";
    return true;
}
