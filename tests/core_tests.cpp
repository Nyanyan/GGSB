#include <iostream>
#include <fstream>
#include <stdexcept>
#include "../src/demo.hpp"
#include "../src/tracking.hpp"
#include "../src/console.hpp"
#include "../src/scroll.hpp"

static int checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) throw std::runtime_error(std::string("line ") + std::to_string(__LINE__) + ": " #value); } while (false)
using namespace ggsb;
int main(int argc, char** argv) {
    try {
        ListScroll scrolling; int list_position = 0;
        CHECK(ListScroll::maximum(0, 9) == 0 && ListScroll::maximum(35, 9) == 26);
        scrolling.wheel(list_position, 35, 9, 0.2, 3); CHECK(list_position == 0);
        scrolling.wheel(list_position, 35, 9, 0.2, 3); CHECK(list_position == 1);
        scrolling.wheel(list_position, 35, 9, 100, 3); CHECK(list_position == 26);
        scrolling.wheel(list_position, 35, 9, -100, 3); CHECK(list_position == 0);
        CHECK(ListScroll::drag(35, 9, 0) == 0 && ListScroll::drag(35, 9, 1) == 26);
        CHECK(ListScroll::drag(35, 9, -10) == 0 && ListScroll::drag(35, 9, 10) == 26);
        list_position = 26; scrolling.wheel(list_position, 3, 9, 0, 3); CHECK(list_position == 0);
        for (int total = 0; total < 100; ++total) for (int visible = 1; visible <= 9; ++visible) {
            CHECK(ListScroll::drag(total, visible, 1) == std::max(0, total - visible));
            CHECK(ListScroll::drag(total, visible, 0) == 0);
        }
        Position p = initial_position();
        CHECK(std::popcount(legal_moves(p)) == 4);
        CHECK(legal_moves(p) == (ggs::cell_bit(19) | ggs::cell_bit(26) | ggs::cell_bit(37) | ggs::cell_bit(44)));
        CHECK(!apply_move(p, false, 0)); CHECK(!apply_move(p, true, -1));
        CHECK(apply_move(p, false, 19)); CHECK(disc_count(p, BLACK) == 4); CHECK(disc_count(p, WHITE) == 1);
        CHECK(p.to_move == WHITE);
        std::mt19937 random(920);
        int pass_count = 0;
        std::vector<Game> graph_games;
        for (int game = 0; game < 64; ++game) {
            Position pos = initial_position();
            Game timeline; timeline.active = timeline.history_ok = true; timeline.history.push_back(pos);
            timeline.evals[0].push_back({0, 0}); timeline.evals[1].push_back({0, 0});
            for (int ply = 0; !is_game_over(pos); ++ply) {
                CHECK(ply < 125); const int cell = choose_move(pos, random);
                const int count = std::popcount(pos.black | pos.white);
                if (cell < 0) ++pass_count;
                CHECK(apply_move(pos, cell < 0, cell));
                CHECK((pos.black & pos.white) == 0);
                CHECK(std::popcount(pos.black | pos.white) == count + (cell >= 0));
                ggs::MoveInfo turn; turn.valid = true; turn.ply = ply + 1; turn.cell = cell; turn.pass = cell < 0;
                timeline.moves.push_back(turn); timeline.history.push_back(pos); timeline.pos = pos; timeline.ply = ply + 1;
                const int placed = std::popcount(pos.black | pos.white) - 4;
                timeline.evals[0].push_back({ply + 1, double(placed)}); timeline.evals[1].push_back({ply + 1, double(placed)});
                CHECK(timeline.placed_at(ply + 1) == placed);
                CHECK(timeline.move_limit() == 60 && timeline.placed() == placed);
                if (cell < 0) CHECK(timeline.placed_at(ply) == timeline.placed_at(ply + 1));
            }
            CHECK(std::abs(score_black(pos)) <= 64);
            CHECK(!apply_move(pos, true, -1));
            const auto curve = timeline.graph_series(BLACK);
            CHECK(curve.size() == size_t(timeline.placed() + 1));
            CHECK(curve.back().first <= 60);
            for (const auto& [x, y] : curve) {
                CHECK(y == x);
                CHECK(timeline.placed_at(timeline.ply_at_placement(x)) == x);
            }
            graph_games.push_back(std::move(timeline));
        }
        CHECK(pass_count > 0);
        Match graph_pair; graph_pair.type = "s8"; graph_pair.player[0] = "left"; graph_pair.player[1] = "right";
        graph_pair.game[0] = graph_games[0]; graph_pair.game[1] = graph_games[1];
        for (int i = 0; i < 2; ++i) { graph_pair.game[i].name[i] = "left"; graph_pair.game[i].name[1 - i] = "right"; }
        CHECK(graph_pair.x_max() == 60);
        for (const auto& [x, y] : graph_pair.graph_series(0)) {
            CHECK(y == x * 2); // Align the two games by placements, even when their passes differ.
            const auto& position = graph_pair.game[0].history[graph_pair.game[0].ply_at_placement(x)];
            CHECK(graph_pair.empty_squares(x) == 64 - std::popcount(position.black | position.white));
        }
        CHECK(graph_range({}, {}) == 4); CHECK(graph_range({{1, 0.8}}, {{2, -3.2}}) == 4);
        CHECK(graph_range({{1, 4.01}}, {}) == 6); CHECK(graph_range({}, {{1, -11.2}}) == 12);
        auto shared_axis = graph_pair;
        shared_axis.game[0].evals[BLACK] = {{1, 1.5}};
        shared_axis.game[0].evals[WHITE] = {{2, -2.0}};
        shared_axis.game[1].evals[BLACK] = {{1, 11.2}};
        shared_axis.game[1].evals[WHITE] = {{2, -6.0}};
        CHECK(shared_axis.graph_range_for(0) == 12);
        CHECK(shared_axis.graph_range_for(1) == 12);
        CHECK(shared_axis.graph_range_for(-1) == 10); // The sum retains its own range.
        shared_axis.type = "8";
        CHECK(shared_axis.graph_range_for(0) == 4);
        auto outcomes = graph_pair;
        CHECK(!outcomes.winner()); // Finished position alone does not finish the match.
        outcomes.game[0].finished = outcomes.game[1].finished = true;
        for (int i = 0; i < 2; ++i) outcomes.game[i].has_official_result = true;
        outcomes.game[0].result_black = 20; outcomes.game[1].result_black = 6;
        CHECK(outcomes.game[0].winner() == BLACK && outcomes.game[1].winner() == BLACK);
        CHECK(outcomes.winner() == 0 && outcomes.result_sum() == 14); // Player 0 is white on board 2.
        outcomes.game[1].result_black = 30;
        CHECK(outcomes.winner() == 1);
        outcomes.game[1].result_black = 20;
        CHECK(outcomes.winner() == -1);
        outcomes.game[0].result_black = 0;
        CHECK(outcomes.game[0].winner() == -1);
        outcomes.has_result = outcomes.finished = true; outcomes.result_p0 = 4;
        CHECK(outcomes.winner() == 0 && outcomes.result_sum() == 8);
        Game aborted; aborted.active = aborted.finished = true; aborted.pos = initial_position();
        CHECK(!aborted.winner());
        Match aborted_match; aborted_match.player[0] = "left"; aborted_match.player[1] = "right";
        aborted_match.finished = aborted_match.closed_event = true;
        aborted.name[BLACK] = "left"; aborted.name[WHITE] = "right"; aborted_match.game[0] = aborted;
        CHECK(!aborted_match.winner() && !aborted_match.result_sum());
        auto missing = graph_pair; missing.game[1].evals[WHITE].clear(); CHECK(missing.graph_series(0).empty());
        missing = graph_pair; missing.game[1].pos = missing.game[1].history[10]; missing.game[1].ply = 10;
        missing.game[1].history.resize(11); missing.game[1].evals[WHITE].resize(11);
        CHECK(missing.graph_series(0).back().first == missing.game[1].placed()); // Do not extend past the slower unfinished board.
        graph_pair.has_result = graph_pair.finished = true; graph_pair.result_p0 = 12;
        graph_pair.game[0].finished = graph_pair.game[1].finished = true;
        CHECK(graph_pair.graph_series(0).back() == (std::pair<int, double>{graph_pair.max_placements(), 24}));
        CHECK(graph_pair.graph_series(1).back().second == -24);
        double value; int integer;
        CHECK(!ggs::parse_double("nan", value)); CHECK(!ggs::parse_double("inf", value));
        CHECK(!ggs::parse_int("9999999999999999", integer));
        CHECK(ggs::parse_hhmmss("1.02:03:04", integer) && integer == 93784);
        ClientModel ratings;
        ratings.receive({"/os: who 2 change: win draw loss matches", {"bob + 2300.5@100.0", "alice - 2200.1@90.0"}, "8"}, 1);
        ratings.receive({"/os: who 2 change: win draw loss matches", {"alice + 2450.7@80.0", "bob x 2100.2@120.0"}, "8r"}, 2);
        CHECK(ratings.users.size() == 2 && ratings.users[0].name == "bob");
        CHECK(ratings.find_user("bob")->rating == 2300.5 && ratings.find_user("bob")->random_rating == 2100.2);
        CHECK(ratings.find_user("alice")->rating == 2200.1 && ratings.find_user("alice")->random_rating == 2450.7);
        CHECK(ratings.find_user("alice")->state == "+" && ratings.find_user("bob")->state == "x");
        ratings.receive({"/os: who 1 change: win draw loss matches", {"alice - 2210.0@90.0"}, "8"}, 3);
        CHECK(ratings.find_user("alice")->rating == 2210 && ratings.find_user("alice")->random_rating == 2450.7);
        CHECK(ratings.find_user("alice")->state == "-" && !ratings.find_user("bob")->rating);
        ratings.receive({"/os: who 1 change: win draw loss matches", {"alice - 2460.0@80.0"}, "8r"}, 4);
        CHECK(ratings.users.size() == 1 && !ratings.find_user("bob"));
        ratings.receive({"/os: who 0", {}, "6"}, 5);
        CHECK(ratings.users.size() == 1 && ratings.find_user("alice")->random_rating == 2460);
        ratings.receive({"/os: who 0", {}, "8r"}, 6);
        CHECK(ratings.find_user("alice")->rating == 2210 && !ratings.find_user("alice")->random_rating);
        CHECK(player_info_command("bob") == "finger bob");
        CHECK(player_info_command("bob", true) == "t /os finger bob");
        CHECK(!player_info_command("") && !player_info_command("bob\nquit") && !player_info_command("bob alice"));
        CHECK(!token_safe("a\nb")); CHECK(supported_type("s8r14")); CHECK(!supported_type("s10r14"));
        ggs::MessageFramer framer;
        const std::string raw = "/os: who 1\r\n|foo + 2300.1@100.0\r\nREADY\r\n";
        for (char c : raw) framer.feed(std::string(1, c), 0);
        auto msgs = framer.take(); CHECK(msgs.size() == 1); CHECK(msgs[0].body.size() == 1);
        ggs::MessageFramer delayed;
        delayed.feed("/os: up", 100); delayed.tick(1000); CHECK(delayed.take().empty());
        delayed.feed("date .1 8\nREADY\n", 1100); CHECK(delayed.take()[0].head == "/os: update .1 8");
        ClientModel model; Demo demo; demo.start(model, 1000);
        CHECK(model.matches.size() == 3); CHECK(model.selected == ".117");
        auto& match = model.matches.at(".117");
        CHECK(match.games() == 2);
        CHECK(match.x_max() == 50 && match.x_max(1) == 50);
        CHECK(match.empty_squares(0) == 50);
        CHECK(match.empty_squares(match.max_placements(1), 1) == 64 - std::popcount(match.game[1].pos.black | match.game[1].pos.white));
        CHECK(match.game[0].initial_discs() == 14);
        CHECK(model.matches.at(".119").x_max() == 60);
        for (auto& g : match.game) { CHECK(g.active); CHECK(g.history_ok); CHECK(g.history.size() == g.moves.size() + 1); CHECK(g.pos == g.history.back()); }
        CHECK(!match.series(0).empty()); CHECK(!match.series(1).empty()); CHECK(match.divergence() > 0);
        CHECK(match.board_series(0, 0).front().second == match.game[0].evals[BLACK].front().value);
        CHECK(match.board_series(1, 0).front().second == match.game[1].evals[WHITE].front().value);
        CHECK(match.board_series(1, 1).front().second == match.game[1].evals[BLACK].front().value);
        CHECK(match.board_series(-1, 0).empty()); CHECK(match.board_series(2, 0).empty());
        model.username = match.game[0].name[match.game[0].pos.to_move];
        CHECK(model.may_play(match, 0, true)); CHECK(!model.may_play(match, 0, false)); CHECK(!model.may_play(match, 0, true, 5));
        int legal_cell = choose_move(match.game[0].pos, random);
        CHECK(model.move_command(match, 0, legal_cell, true).has_value());
        CHECK(!model.move_command(match, 0, legal_cell, false)); CHECK(!model.move_command(match, 0, legal_cell, true, 5));
        CHECK(!model.move_command(match, 0, -1, true));
        model.registered.reset(); CHECK(!model.move_command(match, 0, legal_cell, true));
        model.receive({": who 2 idle online ip-addr hostname", {"someone + 00:01 00:02", model.username + " - 00:01 00:02 127.0.0.1 host <="}}, 1000);
        CHECK(model.registered == false); CHECK(!model.move_command(match, 0, legal_cell, true));
        model.receive({": who 1 idle online ip-addr hostname", {model.username + " + 00:01 00:02 127.0.0.1 host <="}}, 1000);
        CHECK(model.registered == true); CHECK(model.move_command(match, 0, legal_cell, true).has_value());
        const auto console_size = model.console.size();
        model.receive({"/os: ERR you are not registered.", {}}, 1000);
        CHECK(model.registered == false); CHECK(model.console.size() == console_size);
        model.registered = true;
        model.receive({"alice: you are not registered.", {}}, 1000); CHECK(model.registered == true);
        model.username = "some-observer"; CHECK(!model.move_command(match, 0, legal_cell, true));
        auto saved = export_ggf(match.game[0], match.type); CHECK(!saved.empty());
        Match imported; std::string error;
        CHECK(import_ggf(saved, imported, error)); CHECK(imported.game[0].pos == match.game[0].pos);
        CHECK(imported.game[0].moves.size() == match.game[0].moves.size());
        auto saved_second = export_ggf(match.game[1], match.type);
        CHECK(import_pair_ggf(saved, saved_second, imported, error)); CHECK(imported.games() == 2);
        CHECK(imported.game[0].pos == match.game[0].pos && imported.game[1].pos == match.game[1].pos);
        CHECK(imported.series(0) == match.series(0)); CHECK(imported.divergence() == match.divergence());
        CHECK(imported.x_max() == 50 && imported.graph_series(0) == match.graph_series(0));
        CHECK(imported.graph_divergence() == match.graph_divergence());
        Match custom = match; custom.type = "s8r14";
        custom.game[0] = {}; custom.game[1] = {}; CHECK(custom.x_max() == 50);
        custom.game[0] = match.game[0]; CHECK(custom.x_max() == 50);
        custom.type = "s8r20"; custom.game[0] = custom.game[1] = match.game[0]; CHECK(custom.x_max() == 50); // Actual initial board wins over declared type.
        Game partial; auto partial_message = board_message(match.game[0], match.type, true);
        ggs::BoardMessage parsed; CHECK(ggs::parse_board_message(partial_message, parsed));
        parsed.boards.erase(parsed.boards.begin()); partial.receive(parsed, 1);
        CHECK(!partial.history_ok && partial.initial_discs() == 14 && partial.placed() == match.game[0].placed());
        CHECK(ggs::parse_board_message(board_message(match.game[0], match.type, false), parsed));
        Game random_update; random_update.receive(parsed, 1);
        CHECK(random_update.initial_discs() == 14 && random_update.placed_at(parsed.moves.back().ply) == match.game[0].placed());
        auto gap = graph_games.front(); gap.id = ".555"; gap.name[BLACK] = "left"; gap.name[WHITE] = "right";
        CHECK(ggs::parse_board_message(board_message(gap, "8", false), parsed));
        Game after_gap; after_gap.receive(parsed, 1);
        CHECK(after_gap.placed_at(gap.ply) == gap.placed()); // Recovered update uses board occupancy, not a pass-inclusive ply.
        Game full_initial; full_initial.history = {graph_games[0].pos}; full_initial.pos = full_initial.history.front();
        CHECK(full_initial.move_limit() == 64 - std::popcount(full_initial.pos.black | full_initial.pos.white));
        CHECK(valid_clock("15:00/00:02/00:00")); CHECK(valid_clock("15:00,25/00:02/02:00,N5"));
        CHECK(!valid_clock(":::::")); CHECK(!valid_clock("1/2/3/4"));
        CHECK(chat_command("/os", "hello", ".117") == "t /os tell a .117 hello");
        CHECK(chat_command("whisper", "hello", ".117") == "t /os tell o .117 hello");
        CHECK(chat_command("foo", "hello", "") == "tell foo hello");
        CHECK(chat_command(".chat", "hello everyone", "") == "tell .chat hello everyone");
        CHECK(conversation_command("whisper", "private hello") == "tell whisper private hello");
        CHECK(conversation_command("C++", "hello") == "tell C++ hello");
        CHECK(conversation_command("/os", "help") == "tell /os help");
        CHECK(conversation_command("/chess", "help") == "tell /chess help");
        CHECK(!conversation_command("/", "help") && !conversation_command("/os,bob", "help") && !conversation_command("/os\nquit", "help"));
        CHECK(server_error({"/os: ERR illegal move", {}}) && server_error({": ERR unknown user", {}}));
        CHECK(!server_error({"bob: ERROR in my engine", {}}) && !server_error({".chat bob: ERR text", {}}));
        CHECK(!conversation_command("bob,carol", "hello") && !conversation_command("bob", "hello\nquit"));
        ChatBook punctuation;
        CHECK(punctuation.receive({"C++: hello: punctuation", {}}) && punctuation.threads.at("C++").messages.front().text == "hello: punctuation");
        CHECK(!chat_command("/os", "hello", "local")); CHECK(!chat_command("foo", "hello\nquit", ""));
        ClientModel chat_model; chat_model.username = "alice";
        CHECK(chat_model.chats.order == (std::vector<std::string>{".chat", ".tourney"}));
        chat_model.receive({".chat bob: hello everyone", {"second line"}}, 1);
        chat_model.receive({".tourney bob: tournament notice", {}}, 1);
        CHECK(chat_model.chats.threads.at(".tourney").messages.back().text == "tournament notice");
        CHECK(chat_model.chats.threads.at(".tourney").unread == 1);
        chat_model.receive({"bob: private reply", {}}, 2);
        chat_model.receive({"carol: another conversation", {}}, 3);
        CHECK(chat_model.chats.threads.size() == 4 && chat_model.chats.selected == ".chat");
        CHECK(chat_model.chats.threads.at(".chat").messages.front().text == "hello everyone\nsecond line");
        CHECK(chat_model.chats.threads.at("bob").messages.front().sender == "bob");
        CHECK(chat_model.chats.threads.at("bob").messages.front().text == "private reply");
        CHECK(chat_model.chats.threads.at("bob").unread == 1 && chat_model.chats.threads.at(".chat").unread == 1);
        CHECK(chat_model.chats.open("bob") && chat_model.chats.threads.at("bob").unread == 0);
        CHECK(chat_model.chats.threads.at("carol").unread == 1);
        chat_model.chats.append("bob", "alice", "my reply", true);
        CHECK(chat_model.chats.threads.at("bob").messages.back().outgoing && chat_model.chats.threads.at("bob").unread == 0);
        CHECK(chat_model.chats.open(".chat") && chat_model.chats.threads.at(".chat").unread == 0);
        CHECK(!chat_model.chats.open("bob,carol") && !chat_model.chats.open(".117") && !chat_model.chats.open("bob\nquit"));
        chat_model.receive({": finger bob", {"name: Bob"}}, 4);
        chat_model.receive({"/os: finger bob", {"rating: 2300"}}, 4);
        chat_model.receive({": + bob .chat", {}}, 4);
        chat_model.receive({"password: ", {}}, 4);
        chat_model.receive({"/os: .117 A 2300 bob: game chat", {}}, 4);
        chat_model.receive({"/os: .118.1 O 2300 carol: observers", {}}, 4);
        CHECK(chat_model.chats.threads.size() == 4 && chat_model.chats.threads.at("bob").messages.size() == 2);
        CHECK(chat_model.match_chat.at(".117").front() == "A 2300 bob: game chat" && chat_model.match_chat.at(".118").front() == "O 2300 carol: observers");
        ClientModel service_chat;
        CHECK(service_chat.chats.open("/os") && service_chat.chats.selected == "/os");
        service_chat.receive({"/os: help", {"command list", "who / match / finger"}}, 1);
        CHECK(service_chat.chats.threads.at("/os").messages.back().sender == "/os");
        CHECK(service_chat.chats.threads.at("/os").messages.back().text == "help\ncommand list\nwho / match / finger");
        service_chat.receive({"/chess: help", {"unopened service"}}, 2);
        CHECK(!service_chat.chats.threads.contains("/chess"));
        CHECK(service_chat.chats.open("/chess"));
        service_chat.receive({"/chess: hello", {}}, 3);
        CHECK(service_chat.chats.threads.at("/chess").messages.back().text == "hello");
        service_chat.receive({"/os: match 1", {".117 2300 bob 2200 carol s8r14 R"}}, 4);
        CHECK(service_chat.matches.at(".117").listed && service_chat.matches.at(".117").player[0] == "bob");
        service_chat.receive({"/os: request 1", {".24 2300 bob 15:00/00:02/00:00 s8r14 R 2200 alice"}}, 5);
        CHECK(service_chat.requests.at(".24").from == "bob");
        service_chat.receive({"/os: .117 A 2300 bob: game chat", {}}, 6);
        CHECK(service_chat.match_chat.at(".117").front() == "A 2300 bob: game chat");
        service_chat.receive({"/os: ERR unknown command", {}}, 7);
        CHECK(service_chat.chats.threads.at("/os").messages.back().text == "ERR unknown command");
        for (int i = 0; i < 510; ++i) chat_model.chats.append("bob", "bob", "message " + std::to_string(i));
        CHECK(chat_model.chats.threads.at("bob").messages.size() == 500 && chat_model.chats.threads.at("bob").first_message == 12);
        CHECK(chat_model.chats.threads.at("bob").messages.front().text == "message 10");
        CHECK(!import_ggf("(;PB[a]PW[b]TY[8]B[A1])", imported, error));
        CHECK(!import_ggf("(;PB[a]PW[b]TY[10])", imported, error));
        Match sum; sum.type = "s8"; sum.player[0] = "left"; sum.player[1] = "right";
        for (int i = 0; i < 2; ++i) {
            auto& g = sum.game[i]; g.active = true; g.name[i] = "left"; g.name[1 - i] = "right";
            g.evals[i] = {{1, 3.0}, {5, 4.0}}; g.evals[1 - i] = {{2, -2.0}, {6, -5.0}};
        }
        CHECK(sum.series(0)[0].second == 6); CHECK(sum.series(1)[0].second == -4);
        CHECK(sum.board_series(0, 0)[0].second == 3); CHECK(sum.board_series(1, 1)[0].second == -2);
        ConsoleBuffer console;
        std::vector<std::u32string> console_lines;
        for (int i = 0; i < 1200; ++i) console_lines.push_back(U"abcdefghij日本語xyz" + std::u32string(200, U'.'));
        console.sync(console_lines, 0); console.viewport(25, 120); CHECK(console.top == 1175); CHECK(console.max_left() > 0);
        console.drag_vertical(0); CHECK(console.top == 0 && !console.follow_tail);
        console.drag_vertical(0.5); CHECK(console.top == 588); console.scroll_by(-25); CHECK(console.top == 563);
        console.drag_horizontal(1); CHECK(console.left == console.max_left()); console.drag_horizontal(-10); CHECK(console.left == 0);
        console.begin_selection({5, 8}); console.extend_selection({6, 13});
        CHECK(console.selection_text() == console_lines[5].substr(8) + U"\n" + console_lines[6].substr(0, 13));
        CHECK(console.hit(0, 11.9).column == 11); CHECK(console.cell_column(0, 13) == 16);
        auto console_copy = console.selection_text(); console_lines.push_back(U"new output"); console.sync(console_lines, 0);
        CHECK(console.top == 563 && console.selection_text() == console_copy); // Incoming output preserves reading and selection.
        console_lines.erase(console_lines.begin(), console_lines.begin() + 3); console.sync(console_lines, 3);
        CHECK(console.top == 560 && console.anchor.row == 2 && console.selection_text() == console_copy);
        console.begin_selection({3, 4}); console.extend_selection({1, 2}); CHECK(console.selection().first == (ConsolePosition{1, 2}));
        console.select_all(); CHECK(console.selection().first == ConsolePosition{}); CHECK(console.selection_text().ends_with(U"new output"));
        console.drag_vertical(1); CHECK(console.follow_tail && console.top == console.max_top());
        console.sync({}, 1201); CHECK(!console.selected() && console.top == 0 && console.left == 0);
        console.sync({U"日本語"}, 0); CHECK(console.follow_tail && console.top == 0);
        ConsoleBuffer wheel_console; wheel_console.sync(console_lines, 0); wheel_console.viewport(25, 120); wheel_console.scroll_to(100);
        for (int i = 0; i < 5; ++i) wheel_console.wheel(0.05, 0.05);
        CHECK(wheel_console.top == 101 && wheel_console.left == 1); // High-resolution wheel deltas accumulate.
        wheel_console.wheel(-0.25, -0.25); CHECK(wheel_console.top == 100 && wheel_console.left == 0);
        wheel_console.wheel(1, 0, true); CHECK(wheel_console.top == 125);
        ConsoleBuffer selection_tail; selection_tail.viewport(1, 10); selection_tail.sync({U"first", U"second"}, 0);
        selection_tail.begin_selection({1, 0}); selection_tail.extend_selection({1, 3}); selection_tail.sync({U"first", U"second", U"incoming"}, 0);
        CHECK(selection_tail.top == 1 && !selection_tail.follow_tail && selection_tail.selection_text() == U"sec");
        sum.game[0].ply = 1; sum.game[1].ply = 0;
        CHECK(!sum.clock_running(0)); CHECK(sum.clock_running(1));
        sum.game[1].finished = true; CHECK(sum.clock_running(0)); sum.game[1].finished = false;
        sum.game[1].evals[WHITE].clear(); CHECK(sum.series(0).empty());
        sum.game[0].history = {initial_position(), p}; sum.game[1].history = {initial_position()};
        sum.game[0].history_ok = sum.game[1].history_ok = true;
        CHECK(sum.divergence() == -1); sum.game[1].history.push_back(initial_position()); CHECK(sum.divergence() == 1);
        Game evaluations; auto initial = initial_position(); ggs::MoveInfo move;
        move.ply = 1; move.cell = 19; move.has_eval = false;
        evaluations.record_eval(BLACK, move, &initial); CHECK(evaluations.evals[BLACK].empty());
        move.has_eval = true; move.eval = 4; evaluations.record_eval(BLACK, move, &initial);
        move.ply = 3; move.has_eval = false; evaluations.record_eval(BLACK, move, &initial);
        CHECK(evaluations.evals[BLACK].back().value == 0);
        move.ply = 5; move.pass = true; move.has_eval = true; move.eval = 100;
        evaluations.record_eval(BLACK, move, &initial); CHECK(evaluations.evals[BLACK].size() == 2);
        Request request;
        CHECK(!parse_request_line("/os: + .3 2200 foo 15:00 8k R 2100 bar", request));
        CHECK(parse_request_line("/os: + .3 2200 foo 15:00//00:02 s8r14 R 2100 bar", request));
        CHECK(request.from == "foo" && request.to == "bar" && request.type == "s8r14");
        model.receive({"/os: + .3 2200 foo 15:00//00:02 s8r14 R 2100 bar", {}}, 1); CHECK(model.requests.count(".3") == 1);
        model.receive({"/os: - .3 2200 foo 15:00//00:02 s8r14 R 2100 bar", {}}, 2); CHECK(model.requests.count(".3") == 0);
        model.receive({"/os: who 1", {"foo + 2300.1@100.0"}}, 3); CHECK(model.users.size() == 1 && model.users[0].name == "foo");
        for (uint64_t time = 3000; time < 120000; time += 1700) demo.tick(model, time);
        CHECK(model.matches.at(".117").complete()); CHECK(model.matches.at(".117").result_sum().has_value());
        CHECK(model.matches.at(".119").games() == 1); CHECK(model.matches.at(".119").game[0].finished);
        Match official = match; official.has_result = official.finished = true; official.result_p0 = 12;
        CHECK(official.series(0).back().second == 24 && official.series(1).back().second == -24);
        ggs::MatchEvent archive_event;
        CHECK(ggs::parse_match_event({"/os: - match .65 2628 alice 2616 bob s8r14 R +4.00 .83353", {}}, archive_event));
        CHECK(archive_event.archive_id == ".83353" && archive_event.result0 == 4);
        PlayerRecords book;
        book.start("interval-1", "server:5000", "alice", "2026-10-09 19:00:00");
        std::string player; std::vector<RecordedResult> results;
        CHECK(parse_history({"/os: history 2 alice", {".100 09 Oct 2026 18:00:00 2400 alice 2300 bob +12.0 s8r14", ".101 09 Oct 2026 18:20:00 2400 alice 2200 carol -8.0 8"}}, player, results));
        CHECK(player == "alice" && results.size() == 2 && results[0].games() == 2);
        book.history("server:5000", player, results);
        CHECK(book.results.size() == 2); CHECK(book.find("interval-1")->baseline == 101);
        CHECK(book.stats(*book.find("interval-1")).empty()); // Historical games before start are excluded.
        auto record = [](const std::string& id, const std::string& p0, const std::string& p1, const std::string& type, double score) {
            RecordedResult r; r.scope = "server:5000"; r.archive_id = id; r.key = r.scope + "|" + id; r.player[0] = p0; r.player[1] = p1; r.type = type; r.result0 = score; r.ended = "now"; return r;
        };
        CHECK(book.live(record(".102", "alice", "bob", "8", 16)));
        CHECK(book.live(record(".103", "bob", "alice", "s8r14", 8))); // alice loses 8 points on average per board.
        CHECK(book.live(record(".104", "alice", "bob", "s8", 0)));
        CHECK(!book.live(record(".104", "alice", "bob", "s8", 0))); // Duplicate close event.
        auto stats = book.stats(*book.find("interval-1")); CHECK(stats.size() == 1);
        CHECK(stats[0].wins == 1 && stats[0].draws == 1 && stats[0].losses == 1);
        CHECK(stats[0].win_rate() == 0.5); CHECK(stats[0].boards == 5 && stats[0].synchro == 2);
        CHECK(stats[0].average_margin() == 0); // (+16 - 2*8 + 2*0) / 5, from alice's perspective.
        book.find("interval-1")->active = false;
        CHECK(book.live(record(".105", "alice", "carol", "8", 64)));
        CHECK(book.find("interval-1")->included.size() == 3); // Stopped sessions do not accumulate.
        book.start("interval-2", "server:5000", "alice", "2026-10-09 20:00:00");
        CHECK(book.stats(*book.find("interval-2")).empty());
        CHECK(book.live(record(".106", "alice", "bob", "8", -64)));
        auto tail = book.stats(*book.find("interval-2")); CHECK(tail.size() == 1 && tail[0].average_margin() == -64);
        OpponentStats weighted;
        weighted.add(record(".901", "alice", "bob", "8", 10), 0);
        weighted.add(record(".902", "bob", "alice", "s8", 2), 1);
        CHECK(weighted.boards == 3 && weighted.average_margin() == 2); // (10 - 2*2) / 3.
        OpponentStats reverse;
        reverse.add(record(".901", "alice", "bob", "8", 10), 1);
        reverse.add(record(".902", "bob", "alice", "s8", 2), 0);
        CHECK(reverse.average_margin() == -2);
        book.history("server:5000", "alice", {record(".106", "alice", "bob", "8", -64)});
        CHECK(book.find("interval-2")->baseline == 106 && book.find("interval-2")->included.size() == 1);
        // A new archive counter seen after reconnect is included; replayed older history is not.
        book.history("server:5000", "alice", {record(".101", "alice", "carol", "8", -8), record(".107", "alice", "carol", "8", 0)});
        CHECK(book.find("interval-2")->included.size() == 2);
        book.start("other-endpoint", "other:5000", "alice", "later");
        CHECK(book.live(record(".108", "alice", "bob", "8", 4)));
        CHECK(book.find("other-endpoint")->included.empty()); // Endpoint isolation.
        CHECK(!book.live(record(".109", "alice", "bob", "8", 65)));
        PlayerRecords restored; const auto serialized = book.serialize(); CHECK(restored.deserialize(serialized));
        CHECK(restored.serialize() == serialized); CHECK(restored.stats(*restored.find("interval-1"))[0].win_rate() == 0.5);
        CHECK(!restored.deserialize("GGSB_RECORDS 1\nR invalid\n")); CHECK(restored.serialize() == serialized);
        CHECK(book.history_csv("server:5000", "alice").find("\"bob\",\"s8\",2,0,0.5,") != std::string::npos);
        CHECK(book.stats_csv(*book.find("interval-1")).find(",3,1,1,1,0.5,5,2,0") != std::string::npos);
        CHECK(book.stats_csv(*book.find("interval-1")).find("average_margin_per_board") != std::string::npos);
        CHECK(restored.stats(*restored.find("interval-1"))[0].average_margin() == 0);
        CHECK(archive_number(".0005") == 5); CHECK(archive_number(".1.0") == -1); CHECK(archive_number(".99999999999999999999") == -1);
        CHECK(!parse_history({"/os: history ERR alice", {}}, player, results));
        CHECK(parse_history({"/os: history 1 alice", {".110 09 Oct 2026 18:20:00 2400 alice 2200 bob +8.0 8k"}}, player, results) && results.empty());
        PlayerRecords race;
        race.start("race", "server:5000", "alice", "start");
        race.history("server:5000", "alice", {record(".200", "alice", "bob", "8", 0)});
        CHECK(race.stats(*race.find("race")).empty());
        CHECK(race.live(record(".200", "alice", "bob", "8", 0))); // Close notification after initial snapshot.
        CHECK(race.stats(*race.find("race"))[0].win_rate() == 0.5);
        race.start("race-new", "server:5000", "alice", "later");
        CHECK(!race.live(record(".200", "alice", "bob", "8", 0)));
        CHECK(race.stats(*race.find("race-new")).empty());
        ClientModel resignation;
        resignation.receive(board_message(match.game[0], "8", true), 1);
        resignation.receive({"/os: - match .117 2384 Kurogane 2297 shiro 8 R +64.00 .120", {}}, 2);
        CHECK(resignation.matches.at(".117").game[0].has_official_result);
        CHECK(resignation.matches.at(".117").game[0].final_score() == 64);
        CHECK(resignation.matches.at(".117").game[0].history_ok);
        resignation.receive({"/os: + match .117 2300 alice 2200 bob s8r14 R", {}}, 3);
        CHECK(!resignation.matches.at(".117").finished && !resignation.matches.at(".117").game[0].active);
        CHECK(!resignation.watches.count(".117") && resignation.matches.at(".117").player[0] == "alice");
        // Optional existing GGS_Stream log: exercise joins, late observations and all updates.
        if (argc > 1) {
            std::ifstream log(argv[1]); CHECK(bool(log)); ClientModel trace; ggs::MessageFramer messages; std::string line; uint64_t time = 1;
            CHECK(trace.chats.open("/os"));
            int boards = 0;
            while (std::getline(log, line)) {
                auto marker = line.find(" RECV "); if (marker == std::string::npos) continue;
                messages.feed(line.substr(marker + 6) + "\n", time++);
                for (auto& message : messages.take()) { ggs::BoardMessage bm; if (ggs::parse_board_message(message, bm)) ++boards; trace.receive(message, time); }
            }
            messages.feed("READY\n", time);
            for (auto& message : messages.take()) trace.receive(message, time);
            CHECK(boards > 50); CHECK(trace.matches.size() >= 3);
            int histories = 0;
            for (const auto& [id, m] : trace.matches) for (int i = 0; i < m.games(); ++i) if (m.game[i].active) { CHECK(m.game[i].history_ok); ++histories; }
            CHECK(histories >= 6);
            CHECK(!trace.chats.threads.at("/os").messages.empty());
            std::cout << "Existing log: " << boards << " board messages, " << histories << " valid histories\n";
        }
        std::ofstream sample("build/sample.ggf"); sample << saved;
        std::ofstream pair0("build/sample_0.ggf"); pair0 << saved;
        std::ofstream pair1("build/sample_1.ggf"); pair1 << saved_second;
        std::cout << "PASS: " << checks << " checks\n"; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
