#pragma once
#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace ggsb {
struct ConsolePosition {
    int row = 0, column = 0;
    auto operator<=>(const ConsolePosition&) const = default;
};
// Read-only text, in character coordinates. A full-width glyph occupies two cells.
class ConsoleBuffer {
    uint64_t origin_ = 0;
    int widest_ = 0;
    double wheel_rows_ = 0, wheel_columns_ = 0;
public:
    std::vector<std::u32string> lines;
    int top = 0, left = 0, rows = 25, columns = 125;
    bool follow_tail = true, selecting = false;
    ConsolePosition anchor, caret;
    static int width(char32_t ch) {
        if (ch == U'\t') return 4;
        return (ch >= 0x1100 && (ch <= 0x115f || ch == 0x2329 || ch == 0x232a ||
            (ch >= 0x2e80 && ch <= 0xa4cf && ch != 0x303f) || (ch >= 0xac00 && ch <= 0xd7a3) ||
            (ch >= 0xf900 && ch <= 0xfaff) || (ch >= 0xfe10 && ch <= 0xfe19) ||
            (ch >= 0xfe30 && ch <= 0xfe6f) || (ch >= 0xff00 && ch <= 0xff60) ||
            (ch >= 0xffe0 && ch <= 0xffe6) || (ch >= 0x1f300 && ch <= 0x1faff) || ch >= 0x20000)) ? 2 : 1;
    }
    int max_top() const { return std::max(0, int(lines.size()) - rows); }
    int max_left() const { return std::max(0, widest_ - columns); }
    int widest() const { return widest_; }
    ConsolePosition clamp(ConsolePosition pos) const {
        if (lines.empty()) return {};
        pos.row = std::clamp(pos.row, 0, int(lines.size()) - 1);
        pos.column = std::clamp(pos.column, 0, int(lines[pos.row].size())); return pos;
    }
    void sync(std::vector<std::u32string> text, uint64_t origin) {
        if (origin < origin_) { top = left = 0; follow_tail = true; anchor = caret = {}; selecting = false; }
        const int removed = int(std::min<uint64_t>(origin >= origin_ ? origin - origin_ : 0, uint64_t(lines.size())));
        if (!follow_tail) top = std::max(0, top - removed);
        anchor.row -= removed; caret.row -= removed;
        if (anchor.row < 0 && caret.row < 0) anchor = caret = {};
        origin_ = origin; lines = std::move(text); widest_ = 0;
        for (const auto& line : lines) { int length = 0; for (auto ch : line) length += width(ch); widest_ = std::max(widest_, length); }
        anchor = clamp(anchor); caret = clamp(caret);
        top = follow_tail ? max_top() : std::clamp(top, 0, max_top()); left = std::clamp(left, 0, max_left());
        if (lines.empty()) { anchor = caret = {}; selecting = false; }
    }
    void viewport(int visible_rows, int visible_columns) {
        rows = std::max(1, visible_rows); columns = std::max(1, visible_columns);
        top = follow_tail ? max_top() : std::clamp(top, 0, max_top()); left = std::clamp(left, 0, max_left());
    }
    void scroll_to(int row) { top = std::clamp(row, 0, max_top()); follow_tail = top == max_top(); }
    void scroll_by(int delta) { scroll_to(top + delta); }
    void wheel(double vertical, double horizontal, bool page = false) {
        wheel_rows_ += vertical * (page ? rows : 4); wheel_columns_ += horizontal * 4;
        const int row_delta = int(std::trunc(wheel_rows_ + std::copysign(1e-9, wheel_rows_)));
        const int column_delta = int(std::trunc(wheel_columns_ + std::copysign(1e-9, wheel_columns_)));
        wheel_rows_ -= row_delta; wheel_columns_ -= column_delta;
        if (row_delta) scroll_by(row_delta);
        left = std::clamp(left + column_delta, 0, max_left());
    }
    void drag_vertical(double fraction) { scroll_to(int(std::lround(std::clamp(fraction, 0.0, 1.0) * max_top()))); }
    void drag_horizontal(double fraction) { left = int(std::lround(std::clamp(fraction, 0.0, 1.0) * max_left())); }
    int cell_column(int row, int character) const {
        if (row < 0 || row >= int(lines.size())) return 0;
        int cells = 0;
        for (int i = 0; i < std::min(character, int(lines[row].size())); ++i) cells += width(lines[row][i]);
        return cells;
    }
    ConsolePosition hit(int row, double cell) const {
        auto pos = clamp({row, 0}); if (lines.empty()) return pos;
        double x = 0;
        for (auto ch : lines[pos.row]) { const int advance = width(ch); if (cell < x + advance * 0.5) break; x += advance; ++pos.column; }
        return pos;
    }
    void begin_selection(ConsolePosition pos, bool extend = false) { caret = clamp(pos); if (!extend) anchor = caret; selecting = true; follow_tail = false; }
    void extend_selection(ConsolePosition pos) { caret = clamp(pos); }
    bool selected() const { return !lines.empty() && anchor != caret; }
    void select_all() { if (lines.empty()) return; anchor = {}; caret = {int(lines.size()) - 1, int(lines.back().size())}; selecting = false; follow_tail = false; }
    std::pair<ConsolePosition, ConsolePosition> selection() const { return anchor < caret ? std::pair{anchor, caret} : std::pair{caret, anchor}; }
    std::u32string selection_text() const {
        if (!selected()) return {};
        const auto [begin, end] = selection(); std::u32string result;
        for (int row = begin.row; row <= end.row; ++row) {
            const int first = row == begin.row ? begin.column : 0;
            const int last = row == end.row ? end.column : int(lines[row].size());
            result += lines[row].substr(first, last - first); if (row != end.row) result += U'\n';
        }
        return result;
    }
};
}
