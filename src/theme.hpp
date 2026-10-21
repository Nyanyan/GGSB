#pragma once
#include <Siv3D.hpp>

namespace ggsb::ui {
enum class Theme { Dark, Light };

struct ThemeColors {
    s3d::ColorF background, panel, raised, line, text, muted;
    s3d::ColorF mint, blue, orange, red, mint_fill, on_accent, overlay, white_stone_outline;
    s3d::ColorF board_fill, board_line, mini_grid, board_hint;
};

inline const ThemeColors dark_colors{
    {0.035, 0.053, 0.074}, {0.035, 0.053, 0.074}, {0.035, 0.053, 0.074},
    {0.14, 0.19, 0.24}, {0.88, 0.93, 0.95}, {0.43, 0.54, 0.61},
    {0.39, 0.91, 0.73}, {0.41, 0.65, 1.0}, {1.0, 0.66, 0.36}, {1.0, 0.40, 0.43},
    {88 / 255.0, 205 / 255.0, 164 / 255.0}, {0.035, 0.053, 0.074},
    {0.01, 0.025, 0.04, 0.83}, {0.95, 0.95, 0.95},
    {0.073, 0.225, 0.189}, {0.19, 0.37, 0.32}, {0.025, 0.12, 0.11}, {0.39, 0.91, 0.73}
};
inline const ThemeColors light_colors{
    {0.955, 0.967, 0.978}, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0},
    {0.69, 0.74, 0.78}, {0.10, 0.15, 0.19}, {0.34, 0.41, 0.46},
    {0.055, 0.43, 0.32}, {0.16, 0.36, 0.73}, {0.58, 0.32, 0.055}, {0.74, 0.17, 0.24},
    {0.07, 0.44, 0.33}, {1.0, 1.0, 1.0}, {0.10, 0.15, 0.19, 0.42}, {0.54, 0.61, 0.65},
    {0.44, 0.74, 0.57}, {0.22, 0.49, 0.34}, {0.22, 0.49, 0.34}, {0.07, 0.28, 0.18}
};
inline Theme current_theme = Theme::Dark;
inline ThemeColors active_colors = dark_colors;
// References keep existing drawing code on the current palette after a switch.
inline const s3d::ColorF &background = active_colors.background, &panel = active_colors.panel, &raised = active_colors.raised;
inline const s3d::ColorF &line = active_colors.line, &text = active_colors.text, &muted = active_colors.muted;
inline const s3d::ColorF &mint = active_colors.mint, &blue = active_colors.blue, &orange = active_colors.orange, &red = active_colors.red;
inline const s3d::ColorF &mint_fill = active_colors.mint_fill, &on_accent = active_colors.on_accent, &overlay = active_colors.overlay;
inline const s3d::ColorF& white_stone_outline = active_colors.white_stone_outline;
inline const s3d::ColorF &board_fill = active_colors.board_fill, &board_line = active_colors.board_line;
inline const s3d::ColorF &mini_grid = active_colors.mini_grid, &board_hint = active_colors.board_hint;
// Last-move markers remain bright against black discs in either theme.
inline const s3d::ColorF board_marker{0.39, 0.91, 0.73};
inline const s3d::ColorF logo_green{88 / 255.0, 205 / 255.0, 164 / 255.0}, logo_black{0.035, 0.053, 0.074};

inline s3d::String theme_name() { return current_theme == Theme::Light ? U"light" : U"dark"; }
inline void select_theme(const s3d::String& name) {
    current_theme = name == U"light" ? Theme::Light : Theme::Dark;
    active_colors = current_theme == Theme::Light ? light_colors : dark_colors;
    s3d::Scene::SetBackground(background);
}
}
