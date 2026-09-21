"""Exercise monitor removal through the real HTTP API, using only an owned DB."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


with tempfile.TemporaryDirectory(prefix="douyin-room-removal-") as directory:
    http_port, ws_port = free_port(), free_port()
    while http_port == ws_port:
        ws_port = free_port()
    base = f"http://127.0.0.1:{http_port}"
    environment = {**os.environ, "DATABASE_PATH": str(Path(directory) / "pipeline.db"),
                   "BIND_HOST": "127.0.0.1", "HTTP_PORT": str(http_port), "WS_PORT": str(ws_port),
                   "INTERNAL_TOKEN": "isolated-room-removal-test"}

    def request(path, method="GET", body=None, expected=200):
        headers = {"Content-Type": "application/json", "X-Internal-Token": environment["INTERNAL_TOKEN"]}
        req = urllib.request.Request(base + path, method=method, headers=headers,
                                     data=None if body is None else json.dumps(body).encode())
        try:
            response = urllib.request.urlopen(req, timeout=2)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            payload = response.read()
            assert response.status == expected, f"{method} {path}: expected {expected}, got {response.status}"
            return json.loads(payload) if payload else None

    def start():
        child = subprocess.Popen([str(Path(sys.argv[1]).resolve())], env=environment,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        try:
            for _ in range(100):
                if child.poll() is not None:
                    raise RuntimeError("Test backend exited before readiness")
                try:
                    request("/health/live")
                    return child
                except OSError:
                    time.sleep(.05)
            raise RuntimeError("Test backend not ready")
        except BaseException:
            child.terminate()
            child.wait(timeout=5)
            raise

    child = start()
    try:
        request("/api/rooms", "POST", {"live_id": "111"}, expected=202)
        request("/api/rooms", "POST", {"live_id": "222"}, expected=202)
        request("/api/rooms/111", "DELETE")
        paused = next(room for room in request("/api/rooms") if room["live_id"] == "111")
        assert not paused["enabled"] and paused["status"] == "stopped", "Pause must keep the room listed"
        removed = request("/api/rooms/111/remove", "POST")
        assert removed == {"live_id": "111", "status": "removed"}
        assert [room["live_id"] for room in request("/api/rooms")] == ["222"], "Removed room stays in the monitor list"
        assert [room["live_id"] for room in request("/internal/targets")] == ["222"], "Removed room stays in collector targets"
        request("/api/rooms/111/remove", "POST")
        request("/api/rooms/999/remove", "POST", expected=404)
        request("/api/rooms/abc/remove", "POST", expected=404)
        assert len(request("/api/rooms")) == 1, "Unknown removal must not create a target"
        request("/api/rooms/111", "DELETE")
        assert len(request("/api/rooms")) == 1, "A late pause must not restore a removed room"
    finally:
        child.terminate()
        child.wait(timeout=5)
    child = start()
    try:
        assert [room["live_id"] for room in request("/api/rooms")] == ["222"], "Removal must survive restart"
        request("/api/rooms", "POST", {"live_id": "111"}, expected=202)
        resumed = next(room for room in request("/api/rooms") if room["live_id"] == "111")
        assert resumed["enabled"] and resumed["status"] == "connecting"
        assert resumed["version"] > paused["version"], "Re-adding must keep target versions increasing"
        request("/api/rooms/111/remove", "POST")
        assert [room["live_id"] for room in request("/internal/targets")] == ["222"], "Removing an enabled room must stop targeting it"
        print("PASS room removal HTTP: pause compatibility, hide/stop, idempotency, restart and explicit re-add")
    finally:
        child.terminate()
        child.wait(timeout=5)
