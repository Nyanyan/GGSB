"""Check connection labels using the language files deployed beside the built executable."""
from pathlib import Path
import argparse
import subprocess

from gui_session_smoke import ROOT, prepare_runtime, user_data_fingerprint


def verify(executable):
    original = user_data_fingerprint()
    runtime = prepare_runtime(executable)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE

    def run(args):
        subprocess.run([str(runtime / "GGSB.exe"), "--mute", *args], cwd=runtime,
                       startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW,
                       check=True, timeout=25)

    result = runtime / "language-result.txt"
    run(["--verify-offline", str(result), "--quit-after", "1"])
    text = result.read_text(encoding="utf-8-sig")
    assert text.startswith("PASS:"), text
    for index, language in enumerate(("en", "ja", "en")):
        capture = ROOT / "build" / f"language-connection-{index}-{language}.png"
        capture.unlink(missing_ok=True)
        run(["--empty", "--screen", "connect", "--language", language,
             "--capture", str(capture), "--capture-at", "1", "--quit-after", "2"])
        assert capture.is_file(), capture
    assert user_data_fingerprint() == original, "Language verification changed the user's settings or accounts"
    print("PASS: build-output language deployment, English/Japanese/English connection labels, isolated runtime")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/Release/GGSB.exe")
    verify(parser.parse_args().exe)
