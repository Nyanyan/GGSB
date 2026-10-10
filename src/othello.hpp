#pragma once
#include <bit>
#include "protocol.hpp"

namespace ggsb {
using ggs::Position;
inline Position initial_position() {
    return {ggs::cell_bit(28) | ggs::cell_bit(35), ggs::cell_bit(27) | ggs::cell_bit(36), BLACK};
}
inline int at(const Position& p, int cell) {
    if (p.black & ggs::cell_bit(cell)) return BLACK;
    if (p.white & ggs::cell_bit(cell)) return WHITE;
    return -1;
}
inline uint64_t flips(const Position& p, int cell) {
    if (cell < 0 || cell >= 64 || p.to_move < 0 || p.to_move > 1 || at(p, cell) >= 0) return 0;
    uint64_t result = 0;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) continue;
        int x = cell % 8 + dx, y = cell / 8 + dy;
        uint64_t captured = 0;
        while (x >= 0 && x < 8 && y >= 0 && y < 8) {
            int c = at(p, y * 8 + x);
            if (c == (p.to_move ^ 1)) captured |= ggs::cell_bit(y * 8 + x);
            else { if (c == p.to_move) result |= captured; break; }
            x += dx; y += dy;
        }
    }
    return result;
}
inline uint64_t legal_moves(const Position& p) {
    uint64_t result = 0;
    for (int i = 0; i < 64; ++i) if (flips(p, i)) result |= ggs::cell_bit(i);
    return result;
}
inline bool is_game_over(Position p) {
    if (legal_moves(p)) return false;
    p.to_move ^= 1;
    return legal_moves(p) == 0;
}
inline bool apply_move(Position& p, bool pass, int cell) {
    if (p.to_move < 0 || p.to_move > 1) return false;
    if (pass) { if (legal_moves(p) || is_game_over(p)) return false; p.to_move ^= 1; return true; }
    uint64_t f = flips(p, cell);
    if (!f) return false;
    auto& me = p.to_move == BLACK ? p.black : p.white;
    auto& op = p.to_move == BLACK ? p.white : p.black;
    me |= f | ggs::cell_bit(cell); op &= ~f;
    p.to_move ^= 1;
    return true;
}
inline int disc_count(const Position& p, int c) { return std::popcount(c == BLACK ? p.black : p.white); }
inline int score_black(const Position& p) {
    int diff = disc_count(p, BLACK) - disc_count(p, WHITE);
    int empty = 64 - std::popcount(p.black | p.white);
    return diff > 0 ? diff + empty : diff < 0 ? diff - empty : 0;
}
inline std::string coordinate(int cell) {
    if (cell < 0) return "PA";
    return std::string(1, char('A' + cell % 8)) + char('1' + cell / 8);
}
}
