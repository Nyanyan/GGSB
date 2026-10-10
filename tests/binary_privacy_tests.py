"""Ensure path sanitization changes diagnostic data without moving executable code."""
from pathlib import Path
import hashlib
import importlib.util
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("privacy", ROOT / "tests/privacy_tests.py")
privacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(privacy)
local_path = "Q" + ":" + "/work/example/"

def fixture():
    data = bytearray(2560)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<H", data, 134, 3)
    struct.pack_into("<H", data, 148, 240)
    struct.pack_into("<H", data, 152, 0x20B)
    for index, (name, offset, flags) in enumerate(((b".text", 1024, 0x60000020), (b".rdata", 1536, 0x40000040), (b".rsrc", 2048, 0x40000040))):
        entry = 392 + index * 40
        data[entry:entry + len(name)] = name
        struct.pack_into("<II", data, entry + 16, 512, offset)
        struct.pack_into("<I", data, entry + 36, flags)
    code = (local_path + "code.cpp").encode()
    data[1024:1024 + len(code)] = code
    strings = (local_path + "diagnostic.hpp\0" + "\nCMake build tool: " + local_path + "cmake.exe\n\0" + "%s://example.invalid/\0").encode()
    data[1536:1536 + len(strings)] = strings
    wide = (local_path + "unicode.cpp\0").encode("utf-16-le")
    data[1792:1792 + len(wide)] = wide
    resource = (local_path + "resource.cpp").encode()
    data[2048:2048 + len(resource)] = resource
    return bytes(data)

def sanitize(path):
    return subprocess.run(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/sanitize-binary.ps1"), "-Path", str(path)], capture_output=True)

def main():
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="binary-privacy-", dir=ROOT / "build") as directory:
        path = Path(directory) / "fixture.exe"
        original = fixture()
        assert privacy.inspect("fixture", original, binary=True), "Audit missed embedded local paths"
        path.write_bytes(original)
        result = sanitize(path)
        assert result.returncode == 0, "Sanitizer failed on the diagnostic fixture"
        cleaned = path.read_bytes()
        assert len(cleaned) == len(original), "File size changed"
        assert cleaned[:1536] == original[:1536], "PE headers or executable code changed"
        assert cleaned[2048:] == original[2048:], "Embedded resources changed"
        assert b"diagnostic.hpp" in cleaned and "unicode.cpp".encode("utf-16-le") in cleaned, "Source filenames lost"
        assert b"%s://example.invalid/" in cleaned, "URL format string changed"
        assert not privacy.inspect("fixture", cleaned, binary=True), "Diagnostic path remains"
        digest = hashlib.sha256(cleaned).digest()
        assert sanitize(path).returncode == 0 and hashlib.sha256(path.read_bytes()).digest() == digest, "Repeated sanitization changed the file"
        signed = bytearray(original)
        struct.pack_into("<I", signed, 152 + 112 + 32, 2304)
        path.write_bytes(signed)
        assert sanitize(path).returncode != 0 and path.read_bytes() == signed, "Signed binary was modified"
        malformed = b"not-an-executable"
        path.write_bytes(malformed)
        assert sanitize(path).returncode != 0 and path.read_bytes() == malformed, "Invalid binary was modified"
    print("PASS: ASCII and Unicode diagnostic paths removed; code, headers, resources, URLs and size preserved; repeated run stable; signed/invalid files protected")

if __name__ == "__main__":
    main()
