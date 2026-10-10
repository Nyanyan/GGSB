#include "../src/accounts.hpp"
#include <iostream>
#include <stdexcept>

static int checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) throw std::runtime_error("account check failed at line " + std::to_string(__LINE__)); } while (false)
static std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
using namespace ggsb;
int main() {
    try {
        const auto root = std::filesystem::path("build/account-tests") / std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const auto file = root / L"保存アカウント.dat";
        AccountVault vault(file); std::string error;
        CHECK(vault.load(error)); CHECK(vault.accounts().empty());
        SavedAccount first{"", "Main account", "SKATGAME.NET", "synthetic-alice-id", "synthetic-password-123!", 5000};
        CHECK(vault.save(first, error)); CHECK(!first.id.empty()); CHECK(first.host == "skatgame.net"); CHECK(vault.preferred() == first.id);
        const auto encrypted = read_bytes(file);
        CHECK(encrypted.starts_with("GGSB_ACCOUNTS_DPAPI_1\n"));
        CHECK(encrypted.find(first.password) == std::string::npos); CHECK(encrypted.find(first.username) == std::string::npos); CHECK(encrypted.find(first.label) == std::string::npos);
        AccountVault reopened(file); CHECK(reopened.load(error)); CHECK(reopened.accounts().size() == 1);
        CHECK(reopened.find(first.id)->password == first.password); CHECK(reopened.find(first.id)->username == first.username);
        SavedAccount watch{"", "Watch account", "skatgame.net", "synthetic-watcher-id", "test-only-quote\"-slash\\-password", 5000};
        CHECK(reopened.save(watch, error)); CHECK(reopened.accounts().size() == 2 && reopened.preferred() == watch.id);
        CHECK(reopened.prefer(first.id, error));
        AccountVault preferred(file); CHECK(preferred.load(error)); CHECK(preferred.preferred() == first.id);
        SavedAccount changed = first; changed.password = "updated-synthetic-password"; changed.label = "Renamed account";
        CHECK(reopened.save(changed, error)); CHECK(reopened.accounts().size() == 2); CHECK(reopened.find(first.id)->password == changed.password);
        SavedAccount duplicate{"", "Same endpoint", "skatgame.net", first.username, "replacement-synthetic-password", 5000};
        CHECK(reopened.save(duplicate, error)); CHECK(duplicate.id == first.id && reopened.accounts().size() == 2);
        SavedAccount other = first; other.id.clear(); other.port = 5001;
        CHECK(reopened.save(other, error)); CHECK(other.id != first.id && reopened.accounts().size() == 3);
        SavedAccount guest{"", "Guest", "127.0.0.1", "guest", "", 5000};
        CHECK(reopened.save(guest, error)); CHECK(reopened.find(guest.id)->password.empty());
        SavedAccount collision = watch; collision.username = first.username;
        CHECK(!reopened.save(collision, error)); CHECK(reopened.accounts().size() == 4);
        SavedAccount invalid = first; invalid.id.clear(); invalid.password = "line\nbreak";
        CHECK(!reopened.save(invalid, error)); invalid.password = std::string("null\0byte", 9); CHECK(!reopened.save(invalid, error));
        invalid.password.clear(); invalid.port = 0; CHECK(!reopened.save(invalid, error)); invalid.port = 70000; CHECK(!reopened.save(invalid, error));
        invalid.port = 5000; invalid.username = "has space"; CHECK(!reopened.save(invalid, error));
        AccountVault all(file); CHECK(all.load(error)); CHECK(all.find(watch.id)->password == watch.password); CHECK(all.find(guest.id)->password.empty());
        CHECK(all.erase(guest.id, error)); CHECK(!all.find(guest.id)); CHECK(!all.preferred().empty());
        CHECK(!all.erase("missing-id", error));
        CHECK(all.erase(first.id, error)); CHECK(all.erase(watch.id, error)); CHECK(all.erase(other.id, error));
        CHECK(all.accounts().empty() && all.preferred().empty());
        AccountVault empty(file); CHECK(empty.load(error) && empty.accounts().empty());
        CHECK(empty.save(first, error));
        const auto intact = read_bytes(file); auto damaged = intact; damaged.back() ^= 0x20;
        { std::ofstream out(file, std::ios::binary); out.write(damaged.data(), static_cast<std::streamsize>(damaged.size())); }
        AccountVault corrupt(file); CHECK(!corrupt.load(error)); CHECK(!corrupt.writable()); CHECK(corrupt.accounts().empty());
        CHECK(!corrupt.save(watch, error)); CHECK(read_bytes(file) == damaged); // Failed decryption never overwrites the vault.
        { std::ofstream out(file, std::ios::binary); out << "not-a-vault"; }
        AccountVault bad_header(file); CHECK(!bad_header.load(error)); CHECK(!bad_header.save(watch, error)); CHECK(read_bytes(file) == "not-a-vault");
        const auto blocker = root / "parent-is-a-file"; { std::ofstream out(blocker); out << "blocker"; }
        AccountVault cannot_write(blocker / "accounts.dat"); CHECK(!cannot_write.save(first, error)); CHECK(cannot_write.accounts().empty());
        std::cout << "PASS: " << checks << " encrypted account checks (save, update, select, restart, delete, integrity, no plaintext, write failure)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
