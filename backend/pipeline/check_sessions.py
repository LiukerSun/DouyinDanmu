"""Exercise session listing and per-session statistics through the real HTTP API."""
import json
import os
from contextlib import closing
from pathlib import Path
import socket
import sqlite3
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


T = 1700000000000

with tempfile.TemporaryDirectory(prefix="douyin-sessions-http-") as directory:
    database = Path(directory) / "pipeline.db"
    http_port, ws_port = free_port(), free_port()
    while ws_port == http_port:
        ws_port = free_port()
    base = f"http://127.0.0.1:{http_port}"
    environment = {**os.environ, "DATABASE_PATH": str(database), "BIND_HOST": "127.0.0.1",
                   "HTTP_PORT": str(http_port), "WS_PORT": str(ws_port)}

    def request(path, body=None, expected=200):
        req = urllib.request.Request(base + path, data=json.dumps(body).encode() if body else None,
                                     headers={"Content-Type": "application/json"})
        try:
            response = urllib.request.urlopen(req, timeout=2)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            payload = response.read()
            assert response.status == expected, f"{path}: expected {expected}, got {response.status}"
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
        request("/api/rooms", {"live_id": "111"}, expected=202)
    finally:
        child.terminate()
        child.wait(timeout=5)
    with closing(sqlite3.connect(database)) as connection:
        events = [
            ("c1", "room-x", "chat", T + 1000, {}),
            ("o1", "room-x", "online_count", T + 2000, {"online_count": 30}),
            ("e1", "room-x", "system", T + 3000, {"content": "直播已结束"}),
            ("l1", "room-y", "like", T + 4000, {"like_count": 7}),
            ("c2", "room-y", "chat", T + 5000, {}),
        ]
        connection.executemany("INSERT INTO events(event_id,live_id,body) VALUES(?,'111',?)", (
            (event_id, json.dumps({"event_id": event_id, "live_id": "111", "room_id": room, "type": kind,
                                   "user_id": "123", "user_name": "观众", "content": "历史 " + event_id,
                                   "timestamp": at, **extra}))
            for event_id, room, kind, at, extra in events))
        connection.execute("UPDATE rooms SET status='collecting' WHERE live_id='111'")
        connection.execute("DELETE FROM schema_migrations WHERE name IN ('analytics_v1','analytics_v2_session_metrics','live_sessions_v1')")
        connection.commit()
    child = start()
    try:
        result = request("/api/rooms/111/sessions")
        assert result["total"] == "2", f"Journal boundaries must split two sessions: {result}"
        latest, first = result["items"]
        assert latest["status"] == "live" and latest["started_at_ms"] == T + 4000 and latest["ended_at_ms"] is None, \
            "A collecting room must keep its last session open"
        assert latest["stats"]["like_count"] == "7" and latest["stats"]["chat_count"] == "1", \
            "Session stats must sum likes and chats inside the window"
        assert first["status"] == "ended" and first["ended_at_ms"] == T + 3000 and first["end_source"] == "backfill", \
            "The labeled end-of-live event must close the first session during backfill"
        assert first["stats"]["peak_online"] == 30 and first["stats"]["chat_count"] == "1", \
            "Backfill must recover peak online and chat counts"
        assert result["meta"]["value_unit"] == "diamond" and result["meta"]["session_basis"] == "observed_broadcast_boundaries"
        request("/api/rooms/111/sessions?from_ms=1", expected=400)
        request("/api/rooms/111/sessions?limit=0", expected=400)
        assert request("/api/rooms/111/sessions?limit=1")["next_offset"] == 1, "Pagination must honor the limit"
        assert request("/api/rooms/999/sessions")["total"] == "0", "Unknown rooms must report zero sessions"
        print("PASS sessions HTTP: backfill split, per-session stats, live session, validation and pagination")
    finally:
        child.terminate()
        child.wait(timeout=5)
