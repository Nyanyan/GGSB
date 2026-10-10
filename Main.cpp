#include <Siv3D.hpp>
#include "src/screens.hpp"
#include "src/preview.hpp"

void Main() {
    FileSystem::ChangeCurrentDirectory(FileSystem::ParentPath(FileSystem::ModulePath()));
    if (!FileSystem::Exists(U"THIRD_PARTY_LICENSES.txt")) {
        TextWriter licenses{U"THIRD_PARTY_LICENSES.txt", TextEncoding::UTF8_NO_BOM};
        if (licenses) for (const auto& license : LicenseManager::EnumLicenses()) {
            licenses.writeln(license.title); licenses.writeln(license.copyright); licenses.writeln(license.text); licenses.writeln(U"\n");
        }
    }
    Window::SetTitle(U"GGSB — Othello / Generic Game Server");
    Window::SetStyle(WindowStyle::Sizable);
    Window::Resize(1372, 900);
    Window::SetMinimumFrameBufferSize({1120, 700});
    Scene::Resize(1524, 1000);
    Scene::SetResizeMode(ResizeMode::Keep);
    Scene::SetBackground(ggsb::ui::background);
    System::SetTerminationTriggers(UserAction::CloseButtonClicked);
    ggsb::Application app;
    FilePath capture;
    double capture_at = 2.0, quit_after = -1.0;
    bool captured = false;
    bool capture_pending = false;
    FilePath verification_path;
    FilePath audio_result;
    double audio_peak = 0;
    double audio_started = -1;
    int audio_frames = 0;
    size_t audio_voices = 0;
    bool audio_due = false;
    bool audio_verified = false;
    FilePath session_result;
    int session_port = 0;
    bool expected_registered = false, expected_rejected = false, session_verified = false, chat_verified = false;
    String session_screen;
    auto chat_screen = [&]() { return session_screen == U"chat" || session_screen == U"global-chat" || session_screen == U"tourney-chat" || session_screen == U"service-chat"; };
    auto chat_destination = [&]() -> std::string { return session_screen == U"global-chat" ? ".chat" : session_screen == U"tourney-chat" ? ".tourney" : session_screen == U"service-chat" ? "/os" : "bob"; };
    auto chat_test_text = [&]() -> std::string { return session_screen == U"global-chat" ? "public" : session_screen == U"tourney-chat" ? "tournament" : "private"; };
    auto chat_sent_text = [&]() -> std::string { return session_screen == U"service-chat" ? "help" : chat_test_text() + " test message"; };
    const auto& args = System::GetCommandLineArgs();
    for (size_t i = 0; i < args.size(); ++i) {
        auto next = [&]() { return i + 1 < args.size() ? args[++i] : String{}; };
        if (args[i] == U"--capture") capture = next();
        else if (args[i] == U"--demo") app.start_demo();
        else if (args[i] == U"--demo-finished" && app.mode == ggsb::Application::Mode::Demo) {
            const uint64_t preview_start = app.now();
            for (int step = 1; step <= 75; ++step) app.demo.tick(app.model, preview_start + step * 1700);
        }
        else if (args[i] == U"--console-preview") app.seed_console_preview();
        else if (args[i] == U"--scroll-preview") app.seed_scroll_preview();
        else if (args[i] == U"--list-start") {
            const int start = Max(0, ParseOr<int>(next(), 0));
            app.list_offset = app.accounts_offset = app.chat_tabs_offset = app.watched_offset = app.sessions_offset = app.records_offset = app.mosaic_offset = app.move_offset = start;
            app.move_follow_tail = false;
        }
        else if (args[i] == U"--verify-offline") verification_path = next();
        else if (args[i] == U"--verify-audio") audio_result = next();
        else if (args[i] == U"--verify-session") { session_port = ParseOr<int>(next(), 0); const auto identity = next(); expected_registered = identity == U"registered"; expected_rejected = identity == U"rejected"; session_result = next(); }
        else if (args[i] == U"--session-screen") session_screen = next();
        else if (args[i] == U"--capture-at") capture_at = ParseOr<double>(next(), 2);
        else if (args[i] == U"--quit-after") quit_after = ParseOr<double>(next(), -1);
        else if (args[i] == U"--screen") {
            auto screen = next();
            app.p.focus.clear();
            if (screen == U"arena") app.page = 0;
            else if (screen == U"lobby") app.page = 1;
            else if (screen == U"requests") { app.page = 1; app.lobby_tab = 1; }
            else if (screen == U"finished") { app.page = 1; app.lobby_tab = 2; }
            else if (screen == U"players") app.page = 2;
            else if (screen == U"console") app.page = 3;
            else if (screen == U"settings") app.page = 4;
            else if (screen == U"multi") app.page = 5;
            else if (screen == U"records") app.page = 6;
            else if (screen == U"history") { app.page = 6; app.records_tab = 1; }
            else if (screen == U"challenge") app.modal = "challenge";
            else if (screen == U"connect") app.show_connection();
            else if (screen == U"chat") app.open_chat("bob");
            else if (screen == U"global-chat") app.open_chat(".chat");
            else if (screen == U"tourney-chat") app.open_chat(".tourney");
            else if (screen == U"service-chat") app.open_chat("/os");
        }
        else if (args[i] == U"--empty") { app.connection.reset(); app.mode = ggsb::Application::Mode::Offline; app.model = {}; }
        else if (args[i] == U"--load") app.load_record(next());
        else if (args[i] == U"--review") app.seek(ParseOr<int>(next(), 10));
        else if (args[i] == U"--graph-board") app.graph_board = Clamp(ParseOr<int>(next(), -1), -1, 1);
        else if (args[i] == U"--single") app.choose_match(".119");
        else if (args[i] == U"--language") app.set_language(next(), false);
        else if (args[i] == U"--mute") app.sound = false;
        else if (args[i] == U"--demo-accounts") { app.seed_account_preview(); app.show_connection(); }
    }
    if (!verification_path.empty()) {
        String result; bool success = app.verify_offline_workflow(result);
        TextWriter writer{verification_path, TextEncoding::UTF8_NO_BOM}; writer.writeln(result);
        if (!success) return;
    }
    if (!audio_result.empty()) {
        const int audio_page = app.page == 5 ? 5 : 0;
        app.start_demo(); app.page = audio_page; app.sound = false;
    }
    // GUI integration tests use a loopback server and synthetic credentials only.
    if (session_port > 0 && !session_result.empty()) {
        app.accounts = ggsb::AccountVault{std::filesystem::path(L"records/gui_smoke/login-fixture-accounts.dat")};
        app.selected_account.clear(); app.account_error.clear(); app.account_label.value.clear(); app.save_login = false;
        app.host.value = U"127.0.0.1"; app.port.value = Format(session_port); app.username.value = U"alice"; app.password.value = U"secret-example";
        app.model = {}; app.model.username = "alice"; app.mode = ggsb::Application::Mode::Live; app.show_connection();
        app.connection = std::make_unique<ggsb::Connection>("127.0.0.1", session_port, "alice", "secret-example");
        app.connection->start();
    }
    while (System::Update()) {
        if (!audio_result.empty() && ++audio_frames == 8) {
            // Warm up font and board rendering before sampling this short sound.
            audio_started = Scene::Time(); app.sound = true;
            auto placed = app.selected()->game[0];
            std::mt19937 random{17}; ggsb::demo_step(placed, random);
            app.model.receive(ggsb::board_message(placed, app.selected()->type, false), app.now());
            audio_due = app.move_sound_due();
        }
        app.tick();
        if (audio_started >= 0 && !audio_verified) {
            audio_voices = Max(audio_voices, GlobalAudio::GetActiveVoiceCount());
            for (const float sample : GlobalAudio::GetSamples()) audio_peak = Max(audio_peak, double(Abs(sample)));
            if (Scene::Time() - audio_started > 0.35) {
                TextWriter result{audio_result, TextEncoding::UTF8_NO_BOM};
                result.writeln(String{audio_peak > 0.015 ? U"PASS: board update produced audible output samples" : U"FAIL: placement audio output is silent"} + U", peak=" + ToString(audio_peak, 4) + U", due=" + Format(audio_due) + U", voices=" + Format(audio_voices) + U", volume=" + Format(GlobalAudio::GetVolume()));
                audio_verified = true;
            }
        }
        if (!session_result.empty() && !session_verified && app.epoch == 0 && app.page != ggsb::Application::ConnectionPage) {
            TextWriter result{session_result}; result.writeln(U"FAIL: connection screen closed before login"); return;
        }
        if (!session_result.empty() && !session_verified && expected_rejected && app.connection && app.connection->status().starts_with("認証に失敗")) {
            const bool success = app.page == ggsb::Application::ConnectionPage && !app.online() && !app.may_match() && app.model.matches.empty();
            TextWriter result{session_result, TextEncoding::UTF8_NO_BOM};
            result.writeln(success ? U"PASS: rejected login stays on connection screen, no match controls" : U"FAIL: rejected login left connection screen");
            if (!success) return;
            session_verified = true;
        }
        if (!session_result.empty() && !session_verified && app.online() && app.model.registered.has_value() && app.model.find_user("bob") && app.model.find_user("bob")->random_rating) {
            const bool success = !expected_rejected && app.page == 1 && app.model.registered == expected_registered && app.may_match() == expected_registered && !app.toast.contains(U"ERR");
            const auto* bob = app.model.find_user("bob");
            if (bob->rating != 2300.0 || bob->random_rating != 2100.2) { TextWriter result{session_result}; result.writeln(U"FAIL: two rating types"); return; }
            if (app.model.chats.threads.contains("password")) { TextWriter result{session_result}; result.writeln(U"FAIL: login prompt became a chat tab"); return; }
            if (success && session_screen == U"settings") {
                app.page = 4;
                const String language = ggsb::ui::language_pack().selected();
                const String alternate = language == U"en" ? U"ja" : U"en";
                app.set_language(alternate);
                const INI changed{U"settings.ini"};
                const bool persisted = changed.getOr<String>(U"view.language", U"") == alternate;
                app.set_language(language);
                const INI restored{U"settings.ini"};
                if (!persisted || restored.getOr<String>(U"view.language", U"") != language) {
                    TextWriter result{session_result}; result.writeln(U"FAIL: settings language persistence after login"); return;
                }
            }
            TextWriter result{session_result, TextEncoding::UTF8_NO_BOM};
            const bool chat_test = chat_screen();
            result.writeln(success ? chat_test ? U"PENDING: chat round trip" : session_screen == U"settings" ? U"PASS: login opens lobby, settings language switch and persistence after login" : U"PASS: login opens lobby, identity, match controls, quiet registration, normal/random ratings" : U"FAIL: login lobby / identity / match controls / registration toast");
            if (!success) return;
            session_verified = true;
            if (session_screen == U"players") app.page = 2;
            else if (session_screen == U"settings") app.page = 4;
            else if (session_screen == U"profile") app.show_player_info("bob");
            else if (session_screen == U"stats") app.show_player_info("bob", true);
            else if (session_screen == U"requests") { app.page = 1; app.lobby_tab = 1; }
            else if (session_screen == U"challenge") app.challenge();
            else if (chat_test) {
                app.open_chat(chat_destination());
                app.chat_drafts[app.model.chats.selected].value = ggsb::ui::str(chat_sent_text());
                app.submit_conversation();
                if (session_screen == U"service-chat") app.refresh();
            }
        }
        if (session_verified && !chat_verified && chat_screen()) {
            const auto target = chat_destination();
            const auto& thread = app.model.chats.threads.at(target);
            const auto reply = session_screen == U"service-chat" ? "help\nservice reply fixture" : chat_test_text() + " reply fixture";
            const bool received = std::any_of(thread.messages.begin(), thread.messages.end(), [&](const auto& entry) { return !entry.outgoing && entry.text == reply; });
            const auto* updated_bob = app.model.find_user("bob");
            const bool service_updates = session_screen != U"service-chat" || (updated_bob && updated_bob->rating == 2401.0 && updated_bob->random_rating == 2502.0 && app.model.matches.contains(".118") && app.model.matches.at(".118").listed && app.model.requests.contains(".25"));
            if (received && service_updates) {
                const bool sent = std::any_of(thread.messages.begin(), thread.messages.end(), [&](const auto& entry) { return entry.outgoing && entry.sender == "alice" && entry.text == chat_sent_text(); });
                TextWriter result{session_result, TextEncoding::UTF8_NO_BOM};
                const bool success = sent && app.page == ggsb::Application::ChatPage && app.model.chats.selected == target && app.chat_drafts[target].value.empty();
                result.writeln(success ? U"PASS: login, chat round trip, recipient, sent history, reply, cleared draft" : U"FAIL: chat round trip");
                if (!success) return;
                chat_verified = true;
            }
        }
        if (!session_result.empty() && !session_verified && Scene::Time() > 10) { TextWriter result{session_result}; result.writeln(U"FAIL: login timed out"); return; }
        app.draw();
        if (capture_pending && ScreenCapture::HasNewFrame()) {
            ScreenCapture::GetFrame().save(capture); capture_pending = false;
        }
        if (!capture.isEmpty() && !captured && Scene::Time() >= capture_at) {
            ScreenCapture::RequestCurrentFrame(); capture_pending = true; captured = true;
        }
        if (quit_after >= 0 && Scene::Time() >= quit_after && (audio_result.empty() || audio_verified)) break;
    }
}
