"""Exercise the production Inno download logic with small HTTP fixtures on Windows.

Evidence stays in an explicitly supplied, new directory outside the repository.
The fixture changes install identity, disables registration/shortcuts, and never
launches the game. Download, validation and completion code come from the real ISS.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from build_installer import INNO_SCRIPT, MODEL_SHA256, MODEL_SIZE, PROJECT_ROOT, find_iscc


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--iscc", type=Path)
    parser.add_argument("--existing-model", type=Path,
                        help="Also validate reuse of the real pinned GGUF via a read-only hard link (same volume).")
    args = parser.parse_args()
    evidence = args.out.resolve()
    if evidence == PROJECT_ROOT or PROJECT_ROOT in evidence.parents:
        raise ValueError("Keep test installations and logs outside the repository")
    evidence.mkdir(parents=True, exist_ok=False)
    iscc = find_iscc(args.iscc)
    if not iscc:
        raise RuntimeError("Install Inno Setup before this test")
    stage = evidence / "stage"
    stage.mkdir()
    for name in ("WithinTheLatentWalls.exe", "LICENSE.txt", "INSTALLATION_README.txt"):
        (stage / name).write_text("Installer download fixture\n", encoding="utf-8")
    # Even an accidentally staged model must not be embedded by the ISS wildcard.
    (stage / "must-not-be-embedded.gguf").write_bytes(b"excluded")
    payload = b"GGUF" + bytes(range(256)) * 256
    digest = hashlib.sha256(payload).hexdigest()
    state: dict = {"mode": "valid", "requests": [], "target": evidence}

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            state["requests"].append({
                "game_already_installed": (state["target"] / "WithinTheLatentWalls.exe").is_file(),
                "mode": state["mode"],
            })
            if state["mode"] == "unavailable":
                self.send_error(503)
                return
            body = payload if state["mode"] != "bad-hash" else b"X" * len(payload)
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            if state["mode"] == "interrupted":
                self.wfile.write(body[:100])
                self.wfile.flush()
                self.close_connection = True
            else:
                self.wfile.write(body)

        def log_message(self, format: str, *args: object) -> None:
            pass

    source = INNO_SCRIPT.read_text(encoding="utf-8")
    source = re.sub(r"^AppId=.*$", "AppId=" + str(uuid.uuid4()), source, flags=re.M)
    source = source.replace("SetupLogging=yes", "SetupLogging=yes\nUninstallable=no\nCreateUninstallRegKey=no\nUsePreviousAppDir=no")
    source = source.replace("CloseApplications=yes", "CloseApplications=no")
    source = re.sub(r"^Name: .*localappdata.*\n", "", source, flags=re.M)
    source = re.sub(r"\[Icons\].*?(?=\[Run\])", "", source, flags=re.S)
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    results = []

    def compile_fixture(name: str, text: str, *, size: int = len(payload), sha: str = digest) -> Path:
        script = evidence / f"{name}.iss"
        script.write_text(text, encoding="utf-8-sig")
        command = [str(iscc), "/Q", f"/DStageDir={stage}", f"/DDistDir={evidence}",
                   f"/DAppVersion={name}", "/DModelDir=models\\fixture",
                   "/DModelName=fixture.gguf", f"/DModelHash={sha}",
                   f"/DModelSize={size}",
                   f"/DModelUrl=http://127.0.0.1:{server.server_port}/fixture.gguf", str(script)]
        completed = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=90)
        (evidence / f"{name}-compile.txt").write_text(completed.stdout + completed.stderr, encoding="utf-8")
        if completed.returncode:
            raise RuntimeError(f"Fixture compilation failed: {completed.stdout} {completed.stderr}")
        return evidence / f"Within-the-Latent-Walls-Setup-{name}.exe"

    def check(name: str, setup: Path, *, mode: str = "valid", existing: bytes | None = None,
              success: bool = True, requests: bool = True, target: Path | None = None) -> Path:
        target = target or evidence / name
        target.mkdir(exist_ok=True)
        model = target / "models/fixture/fixture.gguf"
        if existing is not None:
            model.parent.mkdir(parents=True, exist_ok=True)
            model.write_bytes(existing)
        original_mtime = model.stat().st_mtime_ns if model.is_file() else None
        state.update(mode=mode, requests=[], target=target)
        command = [str(setup), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/NOICONS",
                   f"/DIR={target}", f"/LOG={evidence / (name + '.log')}"]
        proc = subprocess.Popen(command, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            code = proc.wait(timeout=90)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], check=False, capture_output=True)
            raise
        record = {"case": name, "exit_code": code, "requests": state["requests"],
                  "model_exists": model.is_file()}
        results.append(record)
        (evidence / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
        assert (code == 0) == success, record
        assert bool(state["requests"]) == requests, record
        assert all(item["game_already_installed"] for item in state["requests"]), record
        assert not (target / "must-not-be-embedded.gguf").exists(), record
        if success:
            assert model.read_bytes() == payload, record
            if not requests:
                assert model.stat().st_mtime_ns == original_mtime, record
        else:
            assert not model.exists(), record
        assert not list(target.rglob("*.tmp")), record
        print(f"PASS {name} (exit={code}, HTTP requests={len(state['requests'])})", flush=True)
        return target

    try:
        setup = compile_fixture("fixture", source)
        check("missing", setup)
        check("existing-valid", setup, existing=payload, requests=False)
        check("existing-corrupt", setup, existing=b"X" * len(payload))
        check("hash-mismatch", setup, mode="bad-hash", success=False)
        check("network-unavailable", setup, mode="unavailable", success=False)
        interrupted = check("interrupted", setup, mode="interrupted", success=False)
        check("retry-after-interruption", setup, target=interrupted)
        # Fault injection simulates the user selecting Skip in the native error
        # dialog: no AfterInstall callback; completion must remain unsuccessful.
        skipped_source = source.replace("Check: ModelDownloadRequired", "Check: SkipRequiredDownload")
        skipped_source += "\nfunction SkipRequiredDownload: Boolean;\nbegin\n  Result := False;\nend;\n"
        skipped = compile_fixture("skip", skipped_source)
        check("skipped-required-model", skipped, success=False, requests=False)
        if args.existing_model:
            # Never use this target for replacement tests: the hard link points
            # to the developer's existing weights. Only validation/reuse is run.
            real_model = args.existing_model.resolve(strict=True)
            assert real_model.stat().st_size == MODEL_SIZE
            target = evidence / "real-model-64bit"
            model = target / "models/fixture/fixture.gguf"
            model.parent.mkdir(parents=True)
            model.hardlink_to(real_model)
            original = model.stat()
            setup = compile_fixture("real-model", source, size=MODEL_SIZE, sha=MODEL_SHA256)
            state.update(mode="unavailable", requests=[], target=target)
            completed = subprocess.run(
                [str(setup), "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/NOICONS",
                 f"/DIR={target}", f"/LOG={evidence / 'real-model-64bit.log'}"],
                creationflags=subprocess.CREATE_NO_WINDOW, timeout=90)
            assert completed.returncode == 0, completed.returncode
            assert not state["requests"], state["requests"]
            assert model.stat().st_ino == original.st_ino
            assert model.stat().st_mtime_ns == original.st_mtime_ns
            results.append({"case": "real-model-64bit", "bytes": MODEL_SIZE,
                            "exit_code": completed.returncode, "requests": []})
            (evidence / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
            print(f"PASS real-model-64bit ({MODEL_SIZE} bytes, no download)", flush=True)
    finally:
        server.shutdown()
        server.server_close()
    print(f"Evidence: {evidence}")


if __name__ == "__main__":
    main()
