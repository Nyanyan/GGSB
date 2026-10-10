#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <set>
#include <algorithm>
#include <chrono>
#pragma comment(lib, "crypt32.lib")

namespace ggsb {
struct SavedAccount {
    std::string id, label, host, username, password;
    int port = 5000;
};
class AccountVault {
    static constexpr size_t max_bytes = 1024 * 1024;
    static constexpr const char* file_header = "GGSB_ACCOUNTS_DPAPI_1\n";
    std::filesystem::path path_;
    std::vector<SavedAccount> accounts_;
    std::string preferred_;
    bool writable_ = true;
    static bool clean(const std::string& s, size_t max_length, bool token = false, bool empty = false) {
        return (empty || !s.empty()) && s.size() <= max_length && s.find_first_of(std::string("\r\n\0", 3)) == std::string::npos
            && (!token || s.find_first_of(" \t") == std::string::npos);
    }
    static bool valid(const SavedAccount& a) {
        return clean(a.id, 128, true, true) && clean(a.label, 512, false, true) && clean(a.host, 128, true)
            && clean(a.username, 128, true) && clean(a.password, 4096, false, true) && a.port > 0 && a.port <= 65535;
    }
    static std::string serialize(const std::vector<SavedAccount>& accounts, const std::string& preferred) {
        std::ostringstream out; out << "GGSB_ACCOUNTS 1\nD " << std::quoted(preferred) << '\n';
        for (const auto& a : accounts) out << "A " << std::quoted(a.id) << ' ' << std::quoted(a.label) << ' ' << std::quoted(a.host) << ' ' << a.port << ' '
            << std::quoted(a.username) << ' ' << std::quoted(a.password) << '\n';
        return out.str();
    }
    static bool parse(const std::string& plain, std::vector<SavedAccount>& accounts, std::string& preferred) {
        std::istringstream in(plain); std::string line; if (!std::getline(in, line) || line != "GGSB_ACCOUNTS 1") return false;
        if (!std::getline(in, line)) return false;
        std::istringstream first(line); char kind; if (!(first >> kind >> std::quoted(preferred)) || kind != 'D') return false;
        first >> std::ws; if (!first.eof()) return false;
        std::set<std::string> ids, endpoints;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            SavedAccount a; std::istringstream row(line);
            if (!(row >> kind >> std::quoted(a.id) >> std::quoted(a.label) >> std::quoted(a.host) >> a.port >> std::quoted(a.username) >> std::quoted(a.password))
                || kind != 'A' || a.id.empty() || !valid(a) || !ids.insert(a.id).second || accounts.size() >= 100) return false;
            row >> std::ws; if (!row.eof()) return false;
            if (!endpoints.insert(a.host + "\t" + std::to_string(a.port) + "\t" + a.username).second) return false;
            accounts.push_back(std::move(a));
        }
        return preferred.empty() || ids.count(preferred);
    }
    bool commit(const std::vector<SavedAccount>& accounts, const std::string& preferred, std::string& error) {
        if (!writable_) { error = "保存ファイルを復号できないため、上書きを止めています"; return false; }
        std::string plain = serialize(accounts, preferred);
        DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(plain.data())}, encrypted{};
        const bool protected_ok = CryptProtectData(&input, L"GGSB saved accounts", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encrypted) != FALSE;
        SecureZeroMemory(plain.data(), plain.size());
        if (!protected_ok) { error = "Windowsによる暗号化に失敗しました"; return false; }
        std::string bytes(file_header); bytes.append(reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData); LocalFree(encrypted.pbData);
        std::error_code ec;
        if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) { error = "アカウントの保存先を作成できませんでした"; return false; }
        const auto temp = std::filesystem::path(path_.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()));
        HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) { error = "アカウントを書き込めませんでした"; return false; }
        DWORD written = 0;
        bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size() && FlushFileBuffers(file);
        CloseHandle(file);
        if (ok) ok = MoveFileExW(temp.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!ok) { DeleteFileW(temp.c_str()); error = "アカウントの保存に失敗しました"; return false; }
        accounts_ = accounts; preferred_ = preferred; error.clear(); return true;
    }
public:
    explicit AccountVault(std::filesystem::path path = "accounts.dat") : path_(std::move(path)) {}
    const std::vector<SavedAccount>& accounts() const { return accounts_; }
    const std::string& preferred() const { return preferred_; }
    bool writable() const { return writable_; }
    const SavedAccount* find(const std::string& id) const {
        auto it = std::find_if(accounts_.begin(), accounts_.end(), [&](const auto& a) { return a.id == id; });
        return it == accounts_.end() ? nullptr : &*it;
    }
    bool load(std::string& error) {
        std::error_code ec; bool exists = std::filesystem::exists(path_, ec);
        if (!exists && !ec) { accounts_.clear(); preferred_.clear(); writable_ = true; error.clear(); return true; }
        writable_ = false;
        const auto size = std::filesystem::file_size(path_, ec);
        if (ec || size > max_bytes || size <= std::char_traits<char>::length(file_header)) { error = "アカウントの保存ファイルを読めませんでした"; return false; }
        std::ifstream input(path_, std::ios::binary); std::string bytes(static_cast<size_t>(size), '\0');
        if (!input.read(bytes.data(), static_cast<std::streamsize>(size)) || !bytes.starts_with(file_header)) { error = "アカウントの保存ファイルの形式が不正です"; return false; }
        const size_t offset = std::char_traits<char>::length(file_header);
        DATA_BLOB encrypted{static_cast<DWORD>(bytes.size() - offset), reinterpret_cast<BYTE*>(bytes.data() + offset)}, decrypted{};
        if (!CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &decrypted)) {
            error = "アカウントを復号できません。同じWindowsユーザーで開いてください"; return false;
        }
        std::string plain(reinterpret_cast<const char*>(decrypted.pbData), decrypted.cbData);
        SecureZeroMemory(decrypted.pbData, decrypted.cbData); LocalFree(decrypted.pbData);
        std::vector<SavedAccount> loaded; std::string preferred;
        const bool ok = parse(plain, loaded, preferred); SecureZeroMemory(plain.data(), plain.size());
        if (!ok) { error = "復号したアカウントの形式が不正です"; return false; }
        accounts_ = std::move(loaded); preferred_ = std::move(preferred); writable_ = true; error.clear(); return true;
    }
    bool save(SavedAccount& account, std::string& error) {
        if (!valid(account)) { error = "ホスト・ポート・ID・保存名を確認してください"; return false; }
        SavedAccount next = account;
        for (char& c : next.host) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        if (next.label.empty()) next.label = next.username + " @ " + next.host;
        auto updated = accounts_;
        auto same = std::find_if(updated.begin(), updated.end(), [&](const auto& a) { return a.host == next.host && a.port == next.port && a.username == next.username; });
        if (next.id.empty() && same != updated.end()) next.id = same->id;
        if (same != updated.end() && same->id != next.id) { error = "同じ接続先とIDのアカウントが既にあります"; return false; }
        if (next.id.empty()) next.id = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + "-" + std::to_string(GetCurrentProcessId());
        auto target = std::find_if(updated.begin(), updated.end(), [&](const auto& a) { return a.id == next.id; });
        if (target == updated.end()) {
            if (updated.size() >= 100) { error = "保存できるアカウントは100件までです"; return false; }
            updated.push_back(next);
        } else *target = next;
        if (!commit(updated, next.id, error)) return false;
        account = std::move(next); return true;
    }
    bool erase(const std::string& id, std::string& error) {
        if (!find(id)) { error = "削除するアカウントを選んでください"; return false; }
        auto updated = accounts_; std::erase_if(updated, [&](const auto& a) { return a.id == id; });
        std::string preferred = preferred_;
        if (preferred == id) preferred = updated.empty() ? "" : updated.front().id;
        return commit(updated, preferred, error);
    }
    bool prefer(const std::string& id, std::string& error) {
        if (!find(id)) return false;
        if (preferred_ == id) { error.clear(); return true; }
        return commit(accounts_, id, error);
    }
};
}
