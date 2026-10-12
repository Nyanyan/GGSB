"""Verify the real installer, installed app, update and data-preserving uninstall."""
from pathlib import Path
import argparse
import hashlib
import subprocess
import uuid
import winreg
import zipfile

ROOT = Path(__file__).resolve().parents[1]
UNINSTALL_KEY = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\{A4FE299F-5BF8-432F-90CB-3D5C933F9260}_is1"


def registration():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, UNINSTALL_KEY) as key:
            return {name: winreg.QueryValueEx(key, name)[0] for name in ("InstallLocation", "DisplayVersion")}
    except FileNotFoundError:
        return None


def run(executable, arguments, timeout=60):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    result = subprocess.run([str(executable), *arguments], cwd=executable.parent,
                            startupinfo=startup, timeout=timeout)
    assert result.returncode == 0, f"{executable.name} exited with {result.returncode}"


def digest(data):
    return hashlib.sha256(data).digest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("installer", type=Path)
    args = parser.parse_args()
    installer = args.installer.resolve(strict=True)
    assert registration() is None, "GGSB is already installed; use a separate Windows user for this test"
    build = (ROOT / "build").resolve()
    fixture = (build / ("installer-smoke-" + uuid.uuid4().hex)).resolve()
    assert fixture.is_relative_to(build), "Test installation must stay in build/"
    fixture.mkdir()
    installed = fixture / "GGSB"
    common = ["/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/NOICONS",
              "/TASKS=", "/LANG=english", "/DIR=" + str(installed)]

    try:
        run(installer, [*common, "/LOG=" + str(fixture / "install.log")])
        info = registration()
        assert info and Path(info["InstallLocation"]).resolve() == installed, "Wrong installation registration"
        assert installer.name == f"GGSB-{info['DisplayVersion']}-windows-x64-setup.exe", "Wrong registered version"
        with zipfile.ZipFile(build / "GGSB-windows-x64.zip") as archive:
            names = set()
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                name = entry.filename.replace("\\", "/")
                names.add(name)
                assert digest((installed / name).read_bytes()) == digest(archive.read(entry)), f"Payload mismatch: {name}"
        extra = {p.relative_to(installed).as_posix() for p in installed.rglob("*") if p.is_file()} - names
        assert extra == {"unins000.exe", "unins000.dat"}, f"Unexpected installed files: {sorted(extra)}"
        print(f"PASS: installed {len(names)} files identical to the distribution ZIP; per-user registration and version correct")

        gui_result = fixture / "gui-smoke.txt"
        run(installed / "GGSB.exe", ["--mute", "--verify-offline", str(gui_result), "--quit-after", "1"], timeout=30)
        result = gui_result.read_text(encoding="utf-8-sig").strip()
        assert result.startswith("PASS:"), result
        print(result)

        saved = {
            "settings.ini": b"[app]\nlanguage = ja\n",
            "accounts.dat": b"installer test account preservation fixture\n",
            "records/installer-test.ggf": b"installer test record preservation fixture\n",
            "logs/installer-test.log": b"installer test log preservation fixture\n",
        }
        for name, data in saved.items():
            path = installed / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        run(installer, [*common, "/LOG=" + str(fixture / "update.log")])
        for name, data in saved.items():
            assert (installed / name).read_bytes() == data, f"Update overwrote {name}"
        print("PASS: reinstall preserves settings, accounts, records and logs")
    finally:
        uninstaller = installed / "unins000.exe"
        if uninstaller.exists():
            run(uninstaller, ["/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                              "/LOG=" + str(fixture / "uninstall.log")])

    assert registration() is None, "Uninstall registration remains"
    for name in names:
        assert not (installed / name).exists(), f"Uninstall left a distribution file: {name}"
    for name, data in saved.items():
        assert (installed / name).read_bytes() == data, f"Uninstall deleted user data: {name}"
    print("PASS: uninstall removes the application and registration, preserving user data")
    print("PASS: installer lifecycle verified; logs retained under " + fixture.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    main()
