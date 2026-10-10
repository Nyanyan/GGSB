"""Check published text, commit history and distribution binaries for local paths."""
from pathlib import Path
import argparse
import re
import subprocess
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TEXT_EXTENSIONS = {".cpp", ".hpp", ".h", ".py", ".ps1", ".md", ".txt", ".json", ".xml", ".svg", ".sln", ".vcxproj"}
PATH_PATTERNS = {
    "absolute Windows path": re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[/\\](?![/\\])[^\s\x00\"<>|]{3,}"),
    "absolute Unix path": re.compile(r"(?<![A-Za-z0-9])/(?:Users|home|mnt|media|Volumes|private|root|opt|tmp)/[^\s\x00\"<>|]+"),
    "network share": re.compile(r"(?<!\\)\\{2,4}[A-Za-z0-9_.-]+\\{1,2}[A-Za-z0-9_$.-]+"),
    "private network address": re.compile(r"\b(?:10(?:\.[0-9]{1,3}){3}|192\.168(?:\.[0-9]{1,3}){2}|172\.(?:1[6-9]|2[0-9]|3[01])(?:\.[0-9]{1,3}){2})\b"),
}

def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True).stdout

def inspect(label, data, binary=False):
    if binary:
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[:2] == b"MZ" and data[pe:pe + 4] == b"PE\0\0", "Invalid executable"
        count = struct.unpack_from("<H", data, pe + 6)[0]
        table = pe + 24 + struct.unpack_from("<H", data, pe + 20)[0]
        values = []
        for index in range(count):
            entry = table + index * 40
            name = data[entry:entry + 8].rstrip(b"\0")
            if name not in {b".rdata", b".data", b".debug"}:
                continue
            size, offset = struct.unpack_from("<II", data, entry + 16)
            region = data[offset:offset + size]
            values.append(region.decode("latin1"))
            values.append(region.decode("utf-16-le", errors="replace"))
            values.append(region.decode("utf-32-le", errors="replace"))
    else:
        values = [data.decode("utf-8-sig")]
    findings = []
    for value in values:
        for kind, pattern in PATH_PATTERNS.items():
            for match in pattern.finditer(value):
                line = value.count("\n", 0, match.start()) + 1
                findings.append(f"{label}:{line}: {kind} [value hidden]")
    return findings

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--history", action="store_true")
    parser.add_argument("--package", type=Path)
    args = parser.parse_args()
    findings, checked = [], 0
    names = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").decode("utf-8").split("\0")
    for name in sorted(set(names) - {""}):
        path = ROOT / name
        if path.suffix.lower() in TEXT_EXTENSIONS or path.name == ".gitignore":
            findings.extend(inspect(name, path.read_bytes()))
            checked += 1
    if args.history:
        seen = set()
        for commit in git("rev-list", "HEAD").decode().splitlines():
            for entry in git("ls-tree", "-r", "-z", commit).split(b"\0"):
                if not entry:
                    continue
                fields, name = entry.split(b"\t", 1)
                name = name.decode("utf-8")
                object_id = fields.split()[2].decode("ascii")
                if object_id in seen or (Path(name).suffix.lower() not in TEXT_EXTENSIONS and name != ".gitignore"):
                    continue
                seen.add(object_id)
                findings.extend(inspect(f"history/{commit[:8]}/{name}", git("cat-file", "blob", object_id)))
                checked += 1
    if args.package:
        with zipfile.ZipFile(args.package) as archive:
            for entry in archive.infolist():
                suffix = Path(entry.filename).suffix.lower()
                if suffix in TEXT_EXTENSIONS or suffix in {".exe", ".dll"}:
                    findings.extend(inspect("package/" + entry.filename, archive.read(entry), suffix in {".exe", ".dll"}))
                    checked += 1
    if findings:
        for finding in sorted(set(findings)):
            print(finding)
        raise SystemExit(1)
    print(f"PASS: {checked} text files or binaries checked; no absolute local paths or private network addresses")

if __name__ == "__main__":
    main()
