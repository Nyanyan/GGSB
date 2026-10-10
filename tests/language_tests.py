"""Validate the shipped UI packs and coverage without translating user content."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]
packs = {language: json.loads((root / f"assets/languages/{language}.json").read_text(encoding="utf-8")) for language in ("en", "ja")}
en, ja = (packs[language]["strings"] for language in ("en", "ja"))
assert en.keys() == ja.keys(), "The language packs have different keys"
assert all(value and not re.search("[ぁ-んァ-ン一-龯]", value) for value in en.values()), "Untranslated English text"
assert ja["ロビー"] == "ロビー" and ja["CONNECTED"] == "接続済み", "Japanese translations missing"
for path in (root / "src").glob("*.hpp"):
    for key in re.findall(r'ui::tr\(U"((?:[^"\\]|\\.)*)"\)', path.read_text(encoding="utf-8")):
        assert key in en, (path, key)
assert en["ロビー"] == "Lobby" and en["言語"] == "Language"
print(f"PASS: {len(en)} entries in English and Japanese, matching keys, UI coverage")
