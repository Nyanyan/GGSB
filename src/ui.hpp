#pragma once
#include <Siv3D.hpp>
#include "model.hpp"
#include "scroll.hpp"
#include "language.hpp"

namespace ggsb::ui {
inline const ColorF background{0.035, 0.053, 0.074}, panel{background}, raised{background};
inline const ColorF line{0.14, 0.19, 0.24}, text{0.88, 0.93, 0.95}, muted{0.43, 0.54, 0.61};
inline const ColorF mint{0.39, 0.91, 0.73}, blue{0.41, 0.65, 1.0}, orange{1.0, 0.66, 0.36}, red{1.0, 0.40, 0.43};
inline String str(const std::string& value) { return Unicode::FromUTF8(value); }
inline String signed_value(double value, int decimals = 1) { return (value >= 0 ? U"+" : U"") + ToString(value, decimals); }
inline String clock_text(int seconds) {
    bool negative = seconds < 0; seconds = Abs(seconds);
    return (negative ? U"−" : U"") + Format(seconds / 60) + U":" + (seconds % 60 < 10 ? U"0" : U"") + Format(seconds % 60);
}
struct Field { String value; size_t cursor = 0; bool all = false, was_composing = false; Field(String v = U"") : value(std::move(v)), cursor(value.size()) {} };

class Painter {
public:
    Font font{FontMethod::MSDF, 48, Typeface::Regular};
    Font bold{FontMethod::MSDF, 48, Typeface::Bold};
    Font mono{FontMethod::MSDF, 48, FileSystem::PathAppend(FileSystem::GetFolderPath(SpecialFolder::SystemFonts), U"consola.ttf")};
    Font mono_jp{FontMethod::MSDF, 48, FileSystem::PathAppend(FileSystem::GetFolderPath(SpecialFolder::SystemFonts), U"msgothic.ttc")};
    Painter() { mono.addFallback(mono_jp); }
    std::string focus;
    std::string previous_focus;
    bool blocked = false, hit_field = false;
    std::vector<std::string> fields;
    std::map<std::string, ListScroll> list_scrolls;
    std::string dragging_list;
    std::string dragging_slider;
    double list_drag_grab = 0;
    void scroll_list(const std::string& id, RectF area, RectF track, int count, int visible, int& offset, double speed = 3) {
        const int maximum = ListScroll::maximum(count, visible);
        offset = Clamp(offset, 0, maximum);
        if (!MouseL.pressed() || blocked) dragging_list.clear();
        const double thumb_height = maximum ? Max(26.0, track.h * visible / Max(1, count)) : track.h;
        RectF thumb{track.x, track.y + (maximum ? (track.h - thumb_height) * offset / maximum : 0), track.w, thumb_height};
        if (!blocked && (area.mouseOver() || track.mouseOver())) list_scrolls[id].wheel(offset, count, visible, Mouse::Wheel(), speed);
        if (!blocked && maximum && track.mouseOver()) {
            Cursor::RequestStyle(CursorStyle::Hand);
            if (MouseL.down()) { dragging_list = id; list_drag_grab = thumb.mouseOver() ? Cursor::PosF().y - thumb.y : thumb_height / 2; hit_field = true; }
        }
        if (dragging_list == id && maximum) {
            offset = ListScroll::drag(count, visible, (Cursor::PosF().y - track.y - list_drag_grab) / (track.h - thumb_height));
            hit_field = true;
        }
        Line{track.center().x, track.y, track.center().x, track.bottomY()}.draw(2, line);
        if (maximum) {
            thumb.y = track.y + (track.h - thumb_height) * offset / maximum;
            thumb.draw(dragging_list == id ? mint : track.mouseOver() ? text : muted);
        }
    }
    void begin() { hit_field = false; fields.clear(); previous_focus = focus; }
    void end() {
        if (MouseL.down() && !hit_field && focus == previous_focus) focus.clear();
        if (KeyTab.down() && !fields.empty()) {
            auto found = std::find(fields.begin(), fields.end(), focus);
            int i = found == fields.end() ? -1 : int(found - fields.begin());
            focus = fields[(i + (KeyShift.pressed() ? int(fields.size()) - 1 : 1)) % fields.size()];
        }
    }
    void label(const String& s, double size, double x, double y, ColorF color = text, bool strong = false) const {
        (strong ? bold : font)(s).draw(size, Vec2{x, y}, color);
    }
    void clipped(const String& s, double size, const RectF& region, ColorF color = text, bool strong = false, bool monospaced = false) const {
        String value = s;
        const auto& f = monospaced ? mono : strong ? bold : font;
        if (f(value).region(size).w > region.w) {
            while (value.size() && f(value + U"…").region(size).w > region.w) value.pop_back();
            value += U"…";
        }
        f(value).draw(size, region.pos, color);
    }
    void card(RectF r, double = 0) const { r.draw(background); }
    bool button(RectF r, String title, bool primary = false, bool enabled = true, ColorF accent = mint) const {
        enabled = enabled && !blocked;
        const bool hover = enabled && r.mouseOver();
        ColorF bg = primary && enabled ? accent : background;
        r.draw(bg).drawFrame(hover ? 1.5 : 1, enabled && (hover || primary) ? accent : line);
        const double size = Min(16.0, 16.0 * (r.w - 16) / Max(1.0, font(title).region(16).w));
        font(title).drawAt(size, r.center(), !enabled ? muted : primary ? background : text);
        if (hover) Cursor::RequestStyle(CursorStyle::Hand);
        return enabled && r.leftClicked();
    }
    bool toggle(RectF r, String title, bool& value) const {
        clipped(title, 18, {r.x, r.y + 5, r.w - 38, r.h - 5});
        RectF track{r.rightX() - 24, r.y + 7, 24, 24};
        track.drawFrame(1, value ? mint : muted);
        if (value) font(U"✓").drawAt(19, track.center(), mint);
        if (!blocked && r.mouseOver()) Cursor::RequestStyle(CursorStyle::Hand);
        if (!blocked && r.leftClicked()) { value = !value; return true; }
        return false;
    }
    bool slider(const std::string& id, RectF r, int& value) {
        if (!blocked) fields.push_back(id);
        const int previous = value;
        if (!MouseL.pressed() || blocked) dragging_slider.clear();
        if (!blocked && r.mouseOver()) {
            Cursor::RequestStyle(CursorStyle::Hand);
            if (MouseL.down()) { dragging_slider = id; focus = id; hit_field = true; }
        }
        const double left = r.x + 7, width = r.w - 14;
        if (dragging_slider == id) { value = Clamp(int(std::lround(100 * (Cursor::PosF().x - left) / width)), 0, 100); hit_field = true; }
        if (!blocked && focus == id) {
            if (KeyLeft.down()) value = Max(0, value - 1);
            if (KeyRight.down()) value = Min(100, value + 1);
            if (KeyHome.down()) value = 0;
            if (KeyEnd.down()) value = 100;
        }
        const double x = left + width * value / 100.0;
        Line{left, r.center().y, left + width, r.center().y}.draw(2, line);
        Line{left, r.center().y, x, r.center().y}.draw(2, mint);
        RectF{x - 7, r.center().y - 11, 14, 22}.draw(focus == id || dragging_slider == id ? mint : text);
        return value != previous;
    }
    bool input(const std::string& id, Field& state, RectF r, String placeholder = U"", bool secret = false, bool monospaced = false) {
        const auto& input_font = monospaced ? mono : font;
        if (!blocked) fields.push_back(id);
        if (!blocked && r.leftClicked()) { focus = id; state.cursor = state.value.size(); hit_field = true; }
        bool active = !blocked && focus == id;
        if (active && r.mouseOver() && MouseL.down()) hit_field = true;
        if (!blocked && r.mouseOver()) Cursor::RequestStyle(CursorStyle::IBeam);
        if (active) {
            if (KeyControl.pressed() && KeyA.down()) state.all = true;
            if (KeyControl.pressed() && KeyC.down() && !secret) Clipboard::SetText(state.value);
            if (KeyControl.pressed() && KeyV.down()) {
                String pasted;
                if (Clipboard::GetText(pasted)) {
                    pasted.replace(U"\r", U""); pasted.replace(U"\n", U""); pasted.replace(U"\t", U" ");
                    if (state.all) { state.value.clear(); state.cursor = 0; state.all = false; }
                    state.value.insert(state.cursor, pasted); state.cursor += pasted.size();
                }
            } else {
                String raw = TextInput::GetRawInput();
                if (state.all && (KeyBackspace.down() || KeyDelete.down() || (!raw.empty() && !KeyControl.pressed()))) {
                    state.value.clear(); state.cursor = 0; state.all = false;
                }
                state.cursor = TextInput::UpdateText(state.value, state.cursor, TextInputMode::AllowBackSpaceDelete);
            }
            if (KeyLeft.down()) { state.cursor = state.cursor ? state.cursor - 1 : 0; state.all = false; }
            if (KeyRight.down()) { state.cursor = Min(state.value.size(), state.cursor + 1); state.all = false; }
            if (KeyHome.down()) { state.cursor = 0; state.all = false; }
            if (KeyEnd.down()) { state.cursor = state.value.size(); state.all = false; }
            if (state.value.size() > 2048) state.value.resize(2048);
            state.cursor = Min(state.cursor, state.value.size());
        }
        r.draw(background).drawFrame(active ? 1.5 : 1, active ? mint : line);
        String shown = secret ? String(state.value.size(), U'•') : state.value;
        String editing = active ? TextInput::GetEditingText() : U"";
        shown.insert(Min(state.cursor, shown.size()), secret ? String(editing.size(), U'•') : editing);
        if (shown.empty() && editing.empty()) input_font(placeholder).draw(17, Vec2{r.x + 12, r.y + 10}, muted);
        else {
            RectF content{r.x + 12, r.y + 9, r.w - 24, r.h - 14};
            // Keep the insertion point visible when editing a long command.
            String prefix = shown.substr(0, Min(state.cursor + editing.size(), shown.size()));
            while (active && !prefix.empty() && input_font(prefix).region(17).w > content.w - 6) {
                prefix.erase(prefix.begin()); shown.erase(shown.begin());
            }
            if (active && state.all) RectF{content.x, content.y + 2, Min(content.w, input_font(shown).region(17).w), 23}.draw(ColorF{mint, 0.2});
            clipped(shown, 17, content, text, false, monospaced);
            if (active && std::fmod(Scene::Time(), 1.0) < 0.55 && !state.all)
                Line{content.x + input_font(prefix).region(17).w, content.y + 2, content.x + input_font(prefix).region(17).w, content.y + 23}.draw(1.5, mint);
            if (active && !editing.empty()) Platform::Windows::TextInput::DrawCandidateWindow(font, {r.x, r.bottomY() + 4}, panel, raised, line, text);
        }
        bool submit = active && KeyEnter.down() && editing.empty() && !state.was_composing;
        state.was_composing = !editing.empty();
        return submit;
    }
    void tag(String title, Vec2 pos, ColorF color = mint) const {
        double width = font(title).region(12).w + 18;
        RectF{pos.x, pos.y, width, 24}.draw(background);
        label(title, 12, pos.x + 9, pos.y + 3, color);
    }
    void logo(Vec2 c, double scale = 1) const {
        RectF{c.x - 21 * scale, c.y - 21 * scale, 42 * scale, 42 * scale}.draw(mint);
        Circle{c.x - 7 * scale, c.y - 7 * scale, 7 * scale}.draw(background);
        Circle{c.x + 7 * scale, c.y + 7 * scale, 7 * scale}.draw(background);
        Circle{c.x + 7 * scale, c.y - 7 * scale, 7 * scale}.draw(ColorF{0.96});
        Circle{c.x - 7 * scale, c.y + 7 * scale, 7 * scale}.draw(ColorF{0.96});
    }
};
}
