"""Check ranking and archive responsiveness through the real HTTP/SQLite boundary."""
import concurrent.futures
from contextlib import closing
import gzip
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

    def request(path, body=None, timeout=2):
        req = urllib.request.Request(base + path, data=json.dumps(body).encode() if body else None, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=timeout) as response:
            return json.load(response)

    def start():
        child = subprocess.Popen([str(Path(sys.argv[1]).resolve())], env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                if child.poll() is not None:
                    raise RuntimeError("Test backend exited before readiness")
                try:
                    if request("/health/live").get("status") == "ok":
                        return child
                    time.sleep(.05)
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

    # Rank snapshots stay in the journal. Counting visible chat must not
    # inflate and parse every historical snapshot on each archive request.
    snapshot = gzip.compress(json.dumps({"type": "audience_rank", "audience_ranks": [
        {"user_id": str(i), "user_name": "viewer", "score": i} for i in range(100)
    ]}).encode())
    with closing(sqlite3.connect(database)) as connection:
        connection.execute("""WITH RECURSIVE n(i) AS (VALUES(1) UNION ALL SELECT i+1 FROM n WHERE i<50000)
            INSERT INTO events(event_id,live_id,body) SELECT 'rank-'||i,'111',? FROM n""", (snapshot,))
        connection.commit()
    child = start()
    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            archive = pool.submit(request, "/api/messages/search?view=events&limit=50")
            rooms = pool.submit(request, "/api/rooms")
            assert rooms.result()[0]["live_id"] == "111"
            page = archive.result()
            assert page["total"] == 6000 and len(page["events"]) == 50
            older = request("/api/messages/search?view=events&before=" + page["next_before"])
            assert older["total"] == 6000 and int(older["events"][0]["seq"]) < int(page["next_before"])
            print("PASS archive pages, exact totals and room polling with 50000 compressed rank snapshots", flush=True)

        # Offline writers need no application UDF. Corrections, replacements
        # and rollback must be visible even before background indexing runs.
        with closing(sqlite3.connect(database)) as connection:
            promoted = json.dumps({"event_id": "rank-50000", "type": "chat", "content": "promoted"})
            connection.execute("UPDATE events SET body=? WHERE event_id='rank-50000'", (promoted,))
            connection.execute("INSERT OR REPLACE INTO events(event_id,live_id,body) VALUES('0','111',?)", (snapshot,))
            connection.commit()
            revised = request("/api/messages/search?view=events&limit=1")
            assert revised["total"] == 6000 and revised["events"][0]["event_id"] == "rank-50000"
            connection.execute("DELETE FROM events WHERE event_id='rank-50000'")
            connection.rollback()
            assert request("/api/messages/search?view=events&q=promoted")["total"] == 1
            connection.execute("DELETE FROM events WHERE event_id='rank-50000'")
            connection.commit()
            assert request("/api/messages/search?view=events&limit=1")["total"] == 5999

            # Simulate a bulk repair with many visibility checks pending. A
            # slower archive read must leave the room polling connection free.
            connection.execute("UPDATE events SET body=body WHERE event_id LIKE 'rank-%'")
            connection.commit()
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            scan = pool.submit(request, "/api/messages/search?view=events", timeout=25)
            time.sleep(.05)
            assert request("/api/rooms")[0]["live_id"] == "111"
            assert scan.result()["total"] == 5999
    finally:
        child.terminate()
        child.wait(timeout=5)
