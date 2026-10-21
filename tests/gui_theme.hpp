#pragma once
#include "../src/application.hpp"

namespace ggsb {
inline bool verify_theme_workflow(Application& app, String& result) {
    using namespace ui;
    auto fail = [&](const String& message) { result = U"FAIL: theme / " + message; return false; };
    if (current_theme != Theme::Dark || background != dark_colors.background) return fail(U"dark default");
    const auto original_audio = app.audio;
    app.audio.set_volume(37);
    app.set_language(U"ja");
    for (const String name : {U"light", U"dark"}) {
        app.set_theme(name);
        INI saved{U"settings.ini"};
        if (saved.getOr<String>(U"view.theme", U"") != name) return fail(U"saved selection");
        Application reopened;
        const auto& expected = name == U"light" ? light_colors : dark_colors;
        if (theme_name() != name || background != expected.background || text != expected.text || mint != expected.mint
            || board_fill != expected.board_fill || board_line != expected.board_line || mini_grid != expected.mini_grid || board_hint != expected.board_hint
            || Scene::GetBackground() != expected.background) return fail(U"palette and scene restored after restart");
        if (language_pack().selected() != U"ja" || reopened.audio.volume != 37 || reopened.legal_hints != app.legal_hints)
            return fail(U"other preferences preserved");
        auto luminance = [](const ColorF& color) {
            auto linear = [](double value) { return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
            return 0.2126 * linear(color.r) + 0.7152 * linear(color.g) + 0.0722 * linear(color.b);
        };
        auto contrast = [&](const ColorF& a, const ColorF& b) {
            const double x = luminance(a), y = luminance(b);
            return (Max(x, y) + 0.05) / (Min(x, y) + 0.05);
        };
        for (const auto& ink : {text, muted, mint, blue, orange, red})
            if (contrast(ink, background) < 4.5 || contrast(ink, panel) < 4.5) return fail(U"text contrast");
        for (const auto& fill : {mint, mint_fill, blue, orange, red})
            if (contrast(on_accent, fill) < 4.5) return fail(U"primary button contrast");
        if (contrast(board_hint, board_fill) < 4.5) return fail(U"board label contrast");
    }
    INI invalid{U"settings.ini"};
    invalid.write(U"view", U"theme", U"unknown");
    if (!invalid.save(U"settings.ini")) return fail(U"invalid setting fixture");
    {
        Application reopened;
        if (current_theme != Theme::Dark || background != dark_colors.background) return fail(U"invalid setting fallback");
    }
    app.audio = original_audio;
    app.set_language(U"en");
    app.set_theme(U"light", false);
    if (current_theme != Theme::Light) return fail(U"immediate switch to light");
    app.set_theme(U"dark");
    return true;
}
}
