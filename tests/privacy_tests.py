"""Check source, Git storage and distribution binaries for local paths."""
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

def read_objects(objects):
    """Read loose and packed objects, including unreachable recovery data."""
    requests = "".join(object_id + "\n" for object_id in objects).encode("ascii")
    data = subprocess.run(["git", "cat-file", "--batch"], cwd=ROOT, input=requests,
                          capture_output=True, check=True).stdout
    offset = 0
    for expected in objects:
        end = data.index(b"\n", offset)
        object_id, kind, size = data[offset:end].split()
        assert object_id.decode("ascii") == expected, "Unexpected Git object"
        offset = end + 1
        size = int(size)
        yield expected, kind.decode("ascii"), data[offset:offset + size]
        offset += size + 1
    assert offset == len(data), "Unexpected Git batch output"

def tree_names(data, hash_size):
    offset = 0
    while offset < len(data):
        end = data.index(b"\0", offset)
        _, name = data[offset:end].split(b" ", 1)
        yield name.decode("utf-8")
        offset = end + 1 + hash_size
    assert offset == len(data), "Invalid Git tree"

def decode_text(data):
    if data.startswith((b"\xff\xfe\0\0", b"\0\0\xfe\xff")):
        return data.decode("utf-32")
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    return data.decode("utf-8-sig")

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
        values = [decode_text(data)]
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
    parser.add_argument("--git-objects", action="store_true",
                        help="Inspect all stored objects, including unreachable objects and tree refs")
    parser.add_argument("--forbid-file", action="append", default=[],
                        help="Reject this filename in source, Git trees and the package")
    parser.add_argument("--package", type=Path)
    args = parser.parse_args()
    findings, checked = [], 0
    forbidden = {name.casefold() for name in args.forbid_file}
    def inspect_name(label, name):
        if Path(name).name.casefold() in forbidden:
            findings.append(f"{label}: forbidden file {name}")
    names = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").decode("utf-8").split("\0")
    for name in sorted(set(names) - {""}):
        path = ROOT / name
        if not path.is_file():
            continue
        inspect_name("source", name)
        if path.suffix.lower() in TEXT_EXTENSIONS or path.name == ".gitignore":
            findings.extend(inspect(name, path.read_bytes()))
            checked += 1
    if args.history:
        seen = set()
        revisions = set(git("rev-list", "--all").decode().splitlines())
        revisions.update(git("for-each-ref", "--format=%(objectname)").decode().splitlines())
        for revision in sorted(revisions):
            for entry in git("ls-tree", "-r", "-z", revision + "^{tree}").split(b"\0"):
                if not entry:
                    continue
                fields, name = entry.split(b"\t", 1)
                name = name.decode("utf-8")
                inspect_name(f"history/{revision[:8]}", name)
                object_id = fields.split()[2].decode("ascii")
                if object_id in seen or (Path(name).suffix.lower() not in TEXT_EXTENSIONS and name != ".gitignore"):
                    continue
                seen.add(object_id)
                findings.extend(inspect(f"history/{revision[:8]}/{name}", git("cat-file", "blob", object_id)))
                checked += 1
    if args.git_objects:
        objects = git("cat-file", "--batch-all-objects", "--batch-check=%(objectname)").decode().splitlines()
        hash_size = 32 if git("rev-parse", "--show-object-format").strip() == b"sha256" else 20
        for object_id, kind, data in read_objects(objects):
            label = f"object/{object_id[:8]}"
            if kind == "tree":
                for name in tree_names(data, hash_size):
                    inspect_name(label, name)
                    findings.extend(inspect(label + "/filename", name.encode("utf-8")))
            elif kind in {"blob", "commit", "tag"}:
                if data.startswith(b"MZ"):
                    findings.extend(inspect(label, data, binary=True))
                else:
                    try:
                        decode_text(data)
                    except UnicodeDecodeError:
                        continue
                    findings.extend(inspect(label, data))
            checked += 1
    if args.package:
        with zipfile.ZipFile(args.package) as archive:
            for entry in archive.infolist():
                inspect_name("package", entry.filename)
                suffix = Path(entry.filename).suffix.lower()
                if suffix in TEXT_EXTENSIONS or suffix in {".exe", ".dll"}:
                    findings.extend(inspect("package/" + entry.filename, archive.read(entry), suffix in {".exe", ".dll"}))
                    checked += 1
    if findings:
        for finding in sorted(set(findings)):
            print(finding)
        raise SystemExit(1)
    print(f"PASS: {checked} source files, Git objects or package entries checked; no absolute local paths, private network addresses or forbidden files")

if __name__ == "__main__":
    main()
