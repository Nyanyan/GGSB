#pragma once
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <string_view>

namespace ggsb {
struct SoundSettings {
    int volume = 100;
    bool chat_enabled = false;
    std::map<std::string, bool> chats;

    static std::string key(std::string_view scope, std::string_view target) {
        auto hex = [](std::string_view value) {
            std::string encoded;
            for (unsigned char byte : value) { encoded += "0123456789abcdef"[byte >> 4]; encoded += "0123456789abcdef"[byte & 15]; }
            return encoded;
        };
        return hex(scope) + "_" + hex(target);
    }
    static bool valid_key(const std::string& value) {
        const auto separator = value.find('_');
        if (value.size() > 4096 || separator == std::string::npos || !separator || separator % 2
            || separator + 1 == value.size() || (value.size() - separator - 1) % 2) return false;
        return std::all_of(value.begin(), value.end(), [](char ch) { return ch == '_' || (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); })
            && value.find('_', separator + 1) == std::string::npos;
    }
    void set_volume(int value) { volume = std::clamp(value, 0, 100); }
    double gain() const { return std::clamp(volume, 0, 100) / 100.0; }
    bool chat_enabled_for(const std::string& scope, const std::string& target) const {
        if (const auto found = chats.find(key(scope, target)); found != chats.end()) return found->second;
        return !target.starts_with("/");
    }
    void set_chat(const std::string& scope, const std::string& target, bool enabled) { chats[key(scope, target)] = enabled; }
    bool chat_due(const std::string& scope, const std::set<std::string>& received) const {
        if (!chat_enabled || gain() == 0) return false;
        return std::any_of(received.begin(), received.end(), [&](const auto& target) { return chat_enabled_for(scope, target); });
    }
};
}
