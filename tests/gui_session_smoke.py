"""Native GGSB login checks against a loopback-only GGS fixture."""
from pathlib import Path
import argparse
import socket
import subprocess
import threading
import hashlib
import shutil
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = None


def user_data_fingerprint():
    files = [ROOT / "App/settings.ini"]
    files += list((ROOT / "App").glob("accounts.dat*"))
    files += list((ROOT / "App/records").glob("players.ggsb*"))
    return {str(path): hashlib.sha256(path.read_bytes()).digest() for path in files if path.is_file()}


def prepare_runtime():
    # The app changes its working directory to the executable's directory.
    # A separate cwd alone therefore cannot protect the user's saved settings.
    (ROOT / "build").mkdir(exist_ok=True)
    runtime = Path(tempfile.mkdtemp(prefix="gui-session-runtime-", dir=ROOT / "build"))
    for name in ("GGSB.exe", "THIRD_PARTY_LICENSES.txt"):
        source = ROOT / "App" / name
        if source.is_file():
            shutil.copy2(source, runtime / name)
    for name in ("dll", "engine", "languages"):
        shutil.copytree(ROOT / "App" / name, runtime / name)
    return runtime


def run(registered, screen, screenshot, rejected=False, language=None):
    global RUNTIME
    if RUNTIME is None:
        RUNTIME = prepare_runtime()
    original_user_data = user_data_fingerprint()
    lobby_events = screen == "lobby-events"
    duration = 64 if lobby_events else 3
    commands = []
    command_times = []
    errors = []
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(15)
    port = listener.getsockname()[1]

    def serve():
        try:
            with listener.accept()[0] as client:
                client.settimeout(duration + 15)
                stream = client.makefile("rb")

                def send(text):
                    client.sendall(text.encode("utf-8"))

                def read():
                    return stream.readline().decode("utf-8").strip()

                send("Enter login\r\n")
                assert read() == "alice"
                send("password: ")
                assert read() == "secret-example"
                if rejected:
                    send("\r\nInvalid password\r\n")
                    assert not stream.readline(), "Rejected login sent further commands"
                    return
                send("\r\nREADY\r\n")
                service_seen = False
                while True:
                    raw = stream.readline()
                    if not raw:
                        break
                    command = raw.decode("utf-8").strip()
                    commands.append(command)
                    command_times.append(time.monotonic())
                    if command == "who":
                        marker = "+" if registered else "-"
                        send(f": who 2 idle online ip-addr hostname\n|alice {marker} 00:00 00:01 127.0.0.1 localhost <=\n|bob + 00:00 00:02\nREADY\n")
                        if not registered:
                            send("/os: ERR you are not registered.\nREADY\n")
                    elif command == "chann + .chat .tourney":
                        send(".chat carol: public fixture\nREADY\n")
                        send(".tourney carol: tournament fixture\nREADY\n")
                        send("bob: private fixture\nREADY\n")
                    elif command == "tell bob private test message":
                        send("bob: private reply fixture\nREADY\n")
                    elif command == "tell .chat public test message":
                        send(".chat carol: public reply fixture\nREADY\n")
                    elif command == "tell .tourney tournament test message":
                        send(".tourney carol: tournament reply fixture\nREADY\n")
                    elif command == "tell /os help":
                        service_seen = True
                        send("/os: help\n|service reply fixture\nREADY\n")
                    elif command == "t /os who 8 a":
                        rating = 2401.0 if service_seen else 2300.0
                        send(f"/os: who 2 change: win draw loss matches\n|bob + {rating}@100.0\n|alice - 2200.0@100.0\nREADY\n")
                    elif command == "t /os who 8r a":
                        rating = 2502.0 if service_seen else 2100.2
                        send(f"/os: who 2 change: win draw loss matches\n|alice - 2450.7@80.0\n|bob + {rating}@120.0\nREADY\n")
                    elif command == "finger bob":
                        send(": finger bob\n|login : bob\n|name : Sample Player\n|info : GGS profile fixture\nREADY\n")
                    elif command == "t /os finger bob":
                        send("/os: finger bob\n|rating: 8 2300.0 8r 2100.2\n|wins: 12 draws: 3 losses: 7\nREADY\n")
                    elif command == "t /os request":
                        request_id = ".25" if service_seen else ".24"
                        send(f"/os: request 1\n|{request_id} 2300 bob 15:00/00:02/00:00 s8r14 R 2200 alice\nREADY\n")
                        if lobby_events:
                            # Update an already-loaded lobby without another snapshot query.
                            send("/os: + match .118 2200 alice 2300 bob 8 R\nREADY\n")
                            send("/os: - match .117 2300 bob 2200 carol s8r14 R +16.00 .200\nREADY\n")
                            send("/os: - .24 2300 bob 15:00/00:02/00:00 s8r14 R 2200 alice\nREADY\n")
                            send("/os: + .25 2200 alice 15:00/00:02/00:00 8 R 2300 bob\nREADY\n")
                    elif command == "t /os match":
                        match_id = ".118" if service_seen else ".117"
                        send(f"/os: match 1\n|{match_id} 2300 bob 2200 carol s8r14 R\nREADY\n")
                    else:
                        send("READY\n")
        except Exception as error:
            errors.append(error)
        finally:
            listener.close()

    worker = threading.Thread(target=serve)
    worker.start()
    label = "rejected" if rejected else "registered" if registered else "guest"
    result = ROOT / "build" / f"gui-session-{label}-{screen or 'lobby'}.txt"
    result.unlink(missing_ok=True)
    args = [str(RUNTIME / "GGSB.exe"), "--mute", "--verify-session", str(port), label, str(result),
            "--capture", str(ROOT / "docs" / screenshot), "--capture-at", "2", "--quit-after", str(duration)]
    if screen:
        args += ["--session-screen", screen]
    if language:
        args += ["--language", language]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    process = subprocess.Popen(args, cwd=RUNTIME, creationflags=subprocess.CREATE_NO_WINDOW, startupinfo=startup)
    try:
        assert process.wait(timeout=duration + 20) == 0
    finally:
        if process.poll() is None:
            process.terminate()
        worker.join(timeout=15)
    assert not worker.is_alive(), "Mock server did not finish"
    assert not errors, errors
    assert result.is_file() and result.read_text(encoding="utf-8").startswith("PASS:"), result
    assert user_data_fingerprint() == original_user_data, "GUI test changed the user's settings, accounts or journal"
    match_commands = [command for command in commands if command.startswith(("t /os rated ", "t /os open "))]
    if registered and not rejected:
        assert "t /os rated +" in match_commands and "t /os open 1" in match_commands, commands
    else:
        assert not match_commands, commands
    if not rejected:
        assert "t /os who 8 a" in commands and "t /os who 8r a" in commands, commands
        assert "chann + .chat .tourney" in commands, commands
        assert "t /os notify + *" in commands, commands
        expected_queries = 2 if screen == "service-chat" else 1
        for query in ("t /os match", "t /os who 8 a", "t /os who 8r a", "t /os request"):
            assert commands.count(query) == expected_queries, (query, commands)
    if lobby_events:
        assert "live match start/end and request notifications" in result.read_text(encoding="utf-8"), result
        keepalives = [when for command, when in zip(commands, command_times) if not command]
        assert len(keepalives) == 1 and keepalives[0] - command_times[0] >= 59, commands
        print("PASS: no 30-second lobby polling, live match/request updates, 60-second keepalive")
    if screen == "profile":
        assert "finger bob" in commands and "t /os finger bob" not in commands, commands
    if screen == "stats":
        assert "t /os finger bob" in commands and "finger bob" not in commands, commands
    if screen == "chat":
        assert "tell bob private test message" in commands and "tell .chat public test message" not in commands, commands
        assert "chat round trip" in result.read_text(encoding="utf-8"), result
    if screen == "global-chat":
        assert "tell .chat public test message" in commands and "tell bob private test message" not in commands, commands
        assert "chat round trip" in result.read_text(encoding="utf-8"), result
    if screen == "tourney-chat":
        assert "tell .tourney tournament test message" in commands and "tell .chat public test message" not in commands, commands
        assert "chat round trip" in result.read_text(encoding="utf-8"), result
    if screen == "service-chat":
        assert "tell /os help" in commands, commands
        assert not any(command.startswith("t /os tell ") for command in commands), commands
        assert "chat round trip" in result.read_text(encoding="utf-8"), result
    if screen == "settings":
        assert "settings language switch and persistence after login" in result.read_text(encoding="utf-8"), result
    if screen == "input-history":
        assert "console and chat input history" in result.read_text(encoding="utf-8"), result
        for command in ("t /os help", "t /os stored", "tell bob history for bob", "tell .chat history for .chat",
                        "tell .tourney history for .tourney", "tell /os history for /os",
                        "t /os tell a .117 match history for /os", "t /os tell o .117 match history for whisper"):
            assert command in commands, (command, commands)
        assert not any("unsent " in command for command in commands), commands
    print(f"PASS: {label} native login, {'connection screen' if rejected else 'lobby'}, match controls, {screen or 'default'} screen, user data unchanged")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--lobby-events", action="store_true", help="Verify live lobby notifications and keepalive over 64 seconds")
    mode.add_argument("--input-history", action="store_true", help="Verify command/chat input history against a local server")
    options = parser.parse_args()
    if options.lobby_events:
        run(True, "lobby-events", "../build/lobby-events.png")
        raise SystemExit(0)
    if options.input_history:
        run(True, "input-history", "../build/input-history.png")
        raise SystemExit(0)
    run(False, "", "guest-lobby.png")
    run(False, "players", "guest-players.png")
    run(False, "requests", "guest-requests.png")
    run(True, "", "lobby.png")
    run(True, "players", "registered-players.png")
    run(True, "settings", "settings.png")
    run(False, "settings", "../build/guest-settings.png")
    run(True, "profile", "profile.png")
    run(True, "stats", "server-stats.png")
    run(True, "chat", "private-chat.png")
    run(True, "global-chat", "global-chat.png")
    run(True, "tourney-chat", "tourney-chat.png")
    run(True, "service-chat", "service-chat.png")
    run(False, "service-chat", "guest-service-chat.png")
    run(False, "global-chat", "guest-chat.png")
    run(True, "challenge", "challenge.png")
    run(False, "", "login-failed.png", rejected=True)
