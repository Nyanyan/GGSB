#pragma once
#include <random>
#include "model.hpp"

namespace ggsb {
inline double heuristic(const Position& p, int color) {
    auto me = color == BLACK ? p.black : p.white, op = color == BLACK ? p.white : p.black;
    Position reverse = p; reverse.to_move = color;
    const int my_mobility = std::popcount(legal_moves(reverse)); reverse.to_move ^= 1;
    const int their_mobility = std::popcount(legal_moves(reverse));
    const uint64_t corners = 0x8100000000000081ULL;
    double phase = std::popcount(me | op) / 64.0;
    return 5.5 * (std::popcount(me & corners) - std::popcount(op & corners)) + 0.8 * (my_mobility - their_mobility)
        + (std::popcount(me) - std::popcount(op)) * phase * phase * 0.7;
}
inline int choose_move(const Position& p, std::mt19937& rng) {
    int best = -1; double best_score = -1e9;
    for (int cell = 0; cell < 64; ++cell) if (flips(p, cell)) {
        Position next = p; apply_move(next, false, cell);
        double score = heuristic(next, p.to_move) + std::uniform_real_distribution<double>(-4, 4)(rng);
        if (score > best_score) { best_score = score; best = cell; }
    }
    return best;
}
inline void demo_step(Game& g, std::mt19937& rng) {
    if (g.finished) return;
    int cell = choose_move(g.pos, rng);
    ggs::MoveInfo move; move.valid = true; move.ply = g.ply + 1; move.pass = cell < 0; move.cell = cell;
    move.has_eval = true; move.eval = heuristic(g.pos, g.pos.to_move);
    if (!apply_move(g.pos, move.pass, cell)) { g.finished = true; return; }
    g.moves.push_back(move); g.history.push_back(g.pos); g.ply = move.ply;
    g.finished = is_game_over(g.pos);
}
// Produce real GGS board messages so the offline preview exercises the same parser.
inline ggs::Message board_message(const Game& g, const std::string& type, bool join) {
    ggs::Message result{ "/os: " + std::string(join ? "join " : "update ") + g.id + " " + type, {} };
    auto add_board = [&](const Position& p) {
        result.body.push_back("  A B C D E F G H");
        for (int y = 0; y < 8; ++y) {
            std::string row = std::to_string(y + 1);
            for (int x = 0; x < 8; ++x) row += std::string(" ") + (at(p, y * 8 + x) == BLACK ? '*' : at(p, y * 8 + x) == WHITE ? 'O' : '-');
            row += " " + std::to_string(y + 1); result.body.push_back(row);
        }
        result.body.push_back("  A B C D E F G H");
        result.body.push_back(std::string(p.to_move == BLACK ? "*" : "O") + " to move");
    };
    if (join) add_board(g.history.front());
    auto clock = [&](int color) {
        int sec = std::max(0, 900 - g.ply * 7 - color * 12);
        std::ostringstream line;
        line << g.name[color] << " (" << g.rating[color] << ' ' << (color == BLACK ? '*' : 'O') << ") "
             << sec / 60 << ':' << std::setw(2) << std::setfill('0') << sec % 60 << ",25:00//00:00";
        result.body.push_back(line.str());
    };
    clock(BLACK); clock(WHITE);
    result.body.push_back(std::to_string(g.ply) + " move(s)");
    auto add_move = [&](const ggs::MoveInfo& mv) {
        std::ostringstream line; line << mv.ply << ": " << coordinate(mv.pass ? -1 : mv.cell) << '/' << std::fixed << std::setprecision(2) << mv.eval << "/3.1";
        result.body.push_back(line.str());
    };
    if (join) for (const auto& mv : g.moves) add_move(mv);
    else if (!g.moves.empty()) add_move(g.moves.back());
    add_board(g.pos); return result;
}
class Demo {
    std::mt19937 rng_{20261009};
    std::vector<Match> matches_;
    uint64_t last_step_ = 0;
public:
    void start(ClientModel& model, uint64_t now) {
        model = {}; model.username = "demo"; model.registered = true; matches_.clear(); last_step_ = now;
        const std::string names[6] = {"Kurogane", "shiro", "parity", "Mobility", "corner", "Tempo"};
        for (int k = 0; k < 3; ++k) {
            Match m; m.id = "." + std::to_string(117 + k); m.type = k == 2 ? "8" : "s8r14"; m.flag = "R";
            m.player[0] = names[k * 2]; m.player[1] = names[k * 2 + 1];
            Position start = initial_position();
            if (k < 2) {
                Game opening; opening.pos = start;
                for (int n = 0; n < 10; ++n) demo_step(opening, rng_);
                start = opening.pos;
            }
            for (int i = 0; i < m.games(); ++i) {
                auto& g = m.game[i]; g.id = m.id + (m.games() == 2 ? "." + std::to_string(i) : "");
                g.name[BLACK] = m.player[i]; g.name[WHITE] = m.player[1 - i];
                g.rating[BLACK] = 2384 - i * 87 - k * 36; g.rating[WHITE] = 2297 + i * 87 - k * 36;
                g.pos = start; g.history.push_back(start); g.history_ok = g.active = true;
                for (int n = 0; n < 16 + k * 2; ++n) demo_step(g, rng_);
            }
            model.receive({"/os: + match " + m.id + " 2384 " + m.player[0] + " 2297 " + m.player[1] + " " + m.type + " R", {}}, now);
            for (int i = 0; i < m.games(); ++i) model.receive(board_message(m.game[i], m.type, true), now);
            matches_.push_back(m);
        }
        model.selected = ".117";
        model.users = {{"Kurogane", "+", 2384, 2461}, {"shiro", "-", 2297, 2345}, {"parity", "+", 2254, 2408}, {"Mobility", "+", 2190, 2315}, {"corner", "+", 2040, 2102}, {"Tempo", "-", 1986, 2057}};
        model.requests[".24"] = {".24", "parity", "*", "s8r14", "15:00//00:02", "R"};
    }
    void tick(ClientModel& model, uint64_t now) {
        if (now - last_step_ < 1600) return;
        last_step_ = now;
        for (auto& m : matches_) for (int i = 0; i < m.games(); ++i) {
            auto& g = m.game[i]; if (g.finished) continue;
            demo_step(g, rng_); model.receive(board_message(g, m.type, false), now);
            if (g.finished) {
                std::ostringstream end;
                end << "/os: end " << g.id << " ( " << g.name[BLACK] << " vs. " << g.name[WHITE] << " ) " << std::showpos << score_black(g.pos);
                model.receive({end.str(), {}}, now);
            }
        }
    }
};
}
