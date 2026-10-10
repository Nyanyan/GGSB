#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ggsb {
struct InputDraft {
    std::u32string text;
    size_t cursor = 0;
    bool selected = false;
};

class InputHistory {
    std::vector<std::u32string> entries_;
    size_t position_ = 0;
    InputDraft draft_;
public:
    static constexpr size_t Limit = 100;
    size_t size() const { return entries_.size(); }
    void submitted(std::u32string text) {
        if (text.empty()) return;
        if (entries_.empty() || entries_.back() != text) {
            entries_.push_back(std::move(text));
            if (entries_.size() > Limit) entries_.erase(entries_.begin());
        }
        position_ = entries_.size(); draft_ = {};
    }
    std::optional<InputDraft> recall(bool older, const InputDraft& current) {
        if (entries_.empty()) return {};
        if (older) {
            if (!position_) return {};
            if (position_ == entries_.size()) draft_ = current;
            --position_;
        } else {
            if (position_ == entries_.size()) return {};
            if (++position_ == entries_.size()) return draft_;
        }
        const auto& text = entries_[position_];
        return InputDraft{text, text.size(), false};
    }
};
}
