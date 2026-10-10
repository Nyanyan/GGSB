"""Verify the audit finds data outside HEAD, including dangling recovery objects."""
from pathlib import Path
import contextlib
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("privacy", ROOT / "tests/privacy_tests.py")
privacy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(privacy)

def main():
    build = (ROOT / "build").resolve()
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="privacy-audit-", dir=build) as directory:
        fixture = Path(directory).resolve()
        assert fixture.is_relative_to(build), "Fixture must stay within the build directory"
        privacy.ROOT = fixture
        env = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull,
                   GIT_AUTHOR_NAME="Test", GIT_AUTHOR_EMAIL="test@example.invalid",
                   GIT_COMMITTER_NAME="Test", GIT_COMMITTER_EMAIL="test@example.invalid")
        def git(*args, data=None):
            return subprocess.run(["git", *args], cwd=fixture, env=env, input=data,
                                  capture_output=True, check=True).stdout.strip()
        def audit(*args, fails=False):
            output = io.StringIO()
            old_argv = sys.argv
            sys.argv = ["privacy_tests.py", *args]
            code = 0
            try:
                with contextlib.redirect_stdout(output):
                    try:
                        privacy.main()
                    except SystemExit as error:
                        code = error.code
            finally:
                sys.argv = old_argv
            assert (code != 0) == fails, f"Unexpected audit result for {args}: {output.getvalue()}"
            assert local_path not in output.getvalue(), "Audit disclosed a detected path"
            return output.getvalue()

        local_path = "Q" + ":" + "/work/example/"
        forbidden = "private" + ".md"
        git("init", "--quiet", "--initial-branch=main")
        (fixture / "README.md").write_text("Safe source\n", encoding="utf-8")
        git("add", "README.md")
        git("commit", "--quiet", "-m", "Safe baseline")
        baseline = git("rev-parse", "HEAD")
        audit("--history", "--git-objects", "--forbid-file", forbidden)
        blob = git("hash-object", "-w", "--stdin", data=(local_path + "source.cpp\n").encode())
        git("hash-object", "-w", "--stdin", data=(local_path + "wide.cpp\n").encode("utf-16"))
        tree = git("mktree", data=b"100644 blob " + blob + b"\t" + forbidden.encode() + b"\n")
        commit = git("commit-tree", tree.decode(), "-p", baseline.decode(),
                     data=(local_path + "commit.txt\n").encode())
        audit("--history", "--forbid-file", forbidden)
        output = audit("--git-objects", "--forbid-file", forbidden, fails=True)
        assert "forbidden file" in output and output.count("absolute Windows path") == 3
        recovery = "refs/codex/turn-diffs/test"
        git("update-ref", recovery, tree.decode())
        output = audit("--history", "--forbid-file", forbidden, fails=True)
        assert "forbidden file" in output and "absolute Windows path" in output
        git("update-ref", "refs/heads/secondary", commit.decode())
        audit("--history", "--forbid-file", forbidden, fails=True)
        package = fixture / "fixture.zip"
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr(forbidden, "Safe content")
        audit("--package", str(package), "--forbid-file", forbidden, fails=True)
        git("update-ref", "-d", recovery)
        git("update-ref", "-d", "refs/heads/secondary")
        git("reflog", "expire", "--expire=now", "--expire-unreachable=now", "--all")
        git("gc", "--prune=now")
        audit("--history", "--git-objects", "--forbid-file", forbidden)
        assert git("rev-parse", "HEAD") == baseline, "Audit changed the active branch"
    print("PASS: other branches, tree refs, dangling blobs/commits, Unicode text and package filenames detected; values hidden; pruned storage clean")

if __name__ == "__main__":
    main()
