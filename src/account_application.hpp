#pragma once
#include "application.hpp"

namespace ggsb {
inline void Application::select_account(const std::string& id) {
    const auto* account = accounts.find(id); if (!account) return;
    selected_account = account->id; account_label.value = ui::str(account->label);
    host.value = ui::str(account->host); port.value = Format(account->port); username.value = ui::str(account->username); password.value = ui::str(account->password);
    for (auto* field : {&account_label, &host, &port, &username, &password}) { field->cursor = field->value.size(); field->all = false; }
    save_login = false; p.focus.clear();
}
inline void Application::new_account() {
    selected_account.clear(); account_label.value.clear(); username.value.clear(); password.value.clear();
    account_label.cursor = username.cursor = password.cursor = 0; account_label.all = username.all = password.all = false;
    save_login = false; p.focus = "account-label";
}
inline bool Application::save_account() {
    int port_number = ParseOr<int>(port.value, 0);
    SavedAccount account{selected_account, account_label.value.toUTF8(), host.value.toUTF8(), username.value.toUTF8(), password.value.toUTF8(), port_number};
    if (!accounts.save(account, account_error)) { notify(ui::tr(ui::str(account_error))); return false; }
    selected_account = account.id; account_label.value = ui::str(account.label); host.value = ui::str(account.host); port.value = Format(account.port); account_label.cursor = account_label.value.size();
    notify(ui::tr(U"IDとパスワードを暗号化して保存しました")); return true;
}
inline void Application::delete_account() {
    if (!accounts.erase(selected_account, account_error)) { notify(ui::tr(ui::str(account_error))); return; }
    new_account(); p.focus.clear(); notify(ui::tr(U"保存したアカウントを削除しました"));
}
inline void Application::draw_account_picker(RectF region) {
    using namespace ui;
    p.label(ui::tr(U"保存済みアカウント"), 17, region.x, region.y, text, true);
    if (p.button({region.rightX() - 72, region.y - 2, 72, 30}, ui::tr(U"＋新規"), false, accounts.writable())) new_account();
    const int visible = 5, count = int(accounts.accounts().size());
    RectF list{region.x, region.y + 46, region.w, 404};
    p.scroll_list("accounts", list, {list.rightX() - 10, list.y, 10, 390}, count, visible, accounts_offset, 2);
    for (int i = accounts_offset; i < count && i < accounts_offset + visible; ++i) {
        const auto& a = accounts.accounts()[i]; RectF row{list.x, list.y + (i - accounts_offset) * 78, list.w - 18, 69};
        const bool chosen = a.id == selected_account;
        if (chosen) RectF{row.x, row.y + 7, 3, row.h - 14}.draw(mint);
        p.clipped(str(a.label), 17, {row.x + 13, row.y + 7, row.w - 26, 24}, chosen ? mint : text, true);
        p.clipped(str(a.username), 13, {row.x + 13, row.y + 31, row.w - 26, 20}, text);
        p.clipped(str(a.host) + U":" + Format(a.port), 10, {row.x + 13, row.y + 51, row.w - 26, 15}, muted);
        if (!p.blocked && row.mouseOver()) Cursor::RequestStyle(CursorStyle::Hand);
        if (!p.blocked && p.dragging_list.empty() && row.leftClicked()) select_account(a.id);
    }
    if (!count) p.font(account_error.empty() ? ui::tr(U"保存済みアカウントなし") : tr(str(account_error))).draw(14, RectF{list.x + 8, list.y + 22, list.w - 16, 150}, account_error.empty() ? muted : orange);
    p.label(Format(count) + ui::tr(U"件"), 10, list.x, list.bottomY() + 8, muted);
    p.font(ui::tr(U"Windowsユーザーごとに暗号化してローカル保存します。")).draw(12, RectF{list.x, list.bottomY() + 54, list.w, 65}, muted);
}
inline void Application::seed_account_preview() {
    accounts = AccountVault{std::filesystem::path(L"records/gui_smoke/accounts-preview.dat")};
    SavedAccount main{"", "メイン · サンプル", "skatgame.net", "sample-main", "synthetic-main-password", 5000};
    SavedAccount watch{"", "観戦用 · サンプル", "skatgame.net", "sample-watch", "synthetic-watch-password", 5000};
    SavedAccount local{"", "開発サーバー · サンプル", "127.0.0.1", "guest", "", 5000};
    accounts.save(main, account_error); accounts.save(watch, account_error); accounts.save(local, account_error);
    select_account(main.id);
}
inline bool Application::verify_account_workflow() {
    auto original_accounts = std::move(accounts);
    const auto old_host = host, old_port = port, old_user = username, old_password = password, old_label = account_label;
    const auto old_selected = selected_account, old_error = account_error; const bool old_save_login = save_login;
    accounts = AccountVault{std::filesystem::path(L"records/gui_smoke/accounts.dat")};
    new_account(); host.value = U"SKATGAME.NET"; port.value = U"5000"; username.value = U"gui-account-test"; password.value = U"synthetic-gui-password"; account_label.value = U"GUIテスト";
    bool ok = save_account(); const auto id = selected_account; new_account(); select_account(id);
    ok &= username.value == U"gui-account-test" && password.value == U"synthetic-gui-password" && host.value == U"skatgame.net" && account_label.value == U"GUIテスト";
    AccountVault reopened{std::filesystem::path(L"records/gui_smoke/accounts.dat")}; std::string error;
    ok &= reopened.load(error) && reopened.find(id) && reopened.preferred() == id;
    delete_account(); ok &= accounts.accounts().empty() && password.value.empty() && selected_account.empty();
    accounts = std::move(original_accounts); host = old_host; port = old_port; username = old_user; password = old_password; account_label = old_label;
    selected_account = old_selected; account_error = old_error; save_login = old_save_login; p.focus.clear();
    return ok;
}
}
