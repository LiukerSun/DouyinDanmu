"""Exercise the real HTTP/SQLite boundary with many users and a time filter."""
import concurrent.futures
from contextlib import closing
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.request


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


with tempfile.TemporaryDirectory(prefix="douyin-analytics-http-") as directory:
    database = Path(directory) / "pipeline.db"
    http_port, ws_port = free_port(), free_port()
    while ws_port == http_port:
        ws_port = free_port()
    base = f"http://127.0.0.1:{http_port}"
    environment = {**os.environ, "DATABASE_PATH": str(database), "BIND_HOST": "127.0.0.1", "HTTP_PORT": str(http_port), "WS_PORT": str(ws_port)}

    def request(path, body=None):
        req = urllib.request.Request(base + path, data=json.dumps(body).encode() if body else None, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=2) as response:
            return json.load(response)

    def start():
        child = subprocess.Popen([str(Path(sys.argv[1]).resolve())], env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
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
        request("/api/rooms", {"live_id": "111"})
    finally:
        child.terminate()
        child.wait(timeout=5)
    with closing(sqlite3.connect(database)) as connection:
        connection.executemany("INSERT INTO events(event_id,live_id,body) VALUES(?,'111',?)", (
            (str(i), json.dumps({"event_id": str(i), "user_id": str(10000 + i % 2000), "user_name": "fixture", "type": "chat", "timestamp": 1700000000000 + i}))
            for i in range(6000)))
        connection.execute("DELETE FROM schema_migrations WHERE name='analytics_v1'")
        connection.commit()
    child = start()
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            for suffix in ["", "&from_ms=1700000000000&to_ms=1700000060000", "&from_ms=1700000000000&to_ms=1700000060000&offset=20"]:
                started = time.monotonic()
                ranking = pool.submit(request, "/api/analytics/chat-ranking?room=111&limit=20" + suffix)
                # A UI polling request must keep working while the ranking is computed.
                health = pool.submit(request, "/api/health")
                result = ranking.result(timeout=3)
                health.result(timeout=3)
                assert result["total"] == "2000"
                assert len(result["items"]) == 20
                elapsed = time.monotonic() - started
                assert elapsed < 2, f"Ranking blocked requests for {elapsed:.2f}s"
                print(f"PASS ranking + health ({'time-filtered' if suffix else 'unfiltered'}): {elapsed:.3f}s", flush=True)
    finally:
        child.terminate()
        child.wait(timeout=5)
