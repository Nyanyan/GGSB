#pragma once
#include <Siv3D.hpp>

namespace ggsb::ui {
class LanguagePack {
    s3d::JSON english_, japanese_;
    s3d::String language_ = U"en";
public:
    LanguagePack() : english_(s3d::JSON::Load(U"languages/en.json")), japanese_(s3d::JSON::Load(U"languages/ja.json")) {}
    void select(const s3d::String& language) { language_ = language == U"ja" ? U"ja" : U"en"; }
    const s3d::String& selected() const { return language_; }
    s3d::String text(const s3d::String& key) const {
        const auto& pack = language_ == U"ja" ? japanese_ : english_;
        if (pack.isObject() && pack[U"strings"].hasElement(key)) return pack[U"strings"][key].getOr<s3d::String>(key);
        if (english_.isObject() && english_[U"strings"].hasElement(key)) return english_[U"strings"][key].getOr<s3d::String>(key);
        return key;
    }
    bool available() const { return english_.isObject() && japanese_.isObject(); }
};
inline LanguagePack& language_pack() { static LanguagePack pack; return pack; }
inline s3d::String tr(const s3d::String& key) { return language_pack().text(key); }
// Network status is a fixed message plus a host, account ID or retry delay.
// Only these known prefixes/suffixes are translated; server/user text is untouched.
inline s3d::String connection_status(const std::string& value) {
    auto status = s3d::Unicode::FromUTF8(value);
    for (const auto* key : {U"接続中 · ", U"接続しています · ", U"切断 · "})
        if (status.starts_with(key)) {
            const s3d::String prefix{key};
            auto detail = status.substr(prefix.size());
            const s3d::String suffix{U"秒後に再接続"};
            if (detail.ends_with(suffix)) detail = detail.substr(0, detail.size() - suffix.size()) + tr(suffix);
            return tr(prefix) + detail;
        }
    return tr(status);
}
}
