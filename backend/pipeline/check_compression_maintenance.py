"""Exercise compression and maintenance through HTTP using an owned temporary DB."""
from contextlib import closing, contextmanager
import gzip
import hashlib
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


def varint(value):
    encoded = bytearray()
    while value >= 128:
        encoded.append((value & 127) | 128)
        value >>= 7
    encoded.append(value)
    return bytes(encoded)


def field(number, value):
    if isinstance(value, int):
        return varint(number << 3) + varint(value)
    if isinstance(value, str):
        value = value.encode()
    return varint((number << 3) | 2) + varint(len(value)) + value


def chat_envelope(sequence):
    chat = field(3, "compression fixture " + str(sequence) + " x" * 80)
    message = field(1, "WebcastChatMessage") + field(2, chat) + field(3, sequence)
    payload = field(7, "msg") + field(8, field(1, message))
    values = [1, f"compression-frame-{sequence}", "111", "room-111", "compression-session", sequence,
              1700000000000 + sequence, payload, hashlib.sha256(payload).hexdigest(),
              "douyin", "upstream_frame", 1]
    return b"".join(field(index + 1, value) for index, value in enumerate(values))


def free_port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


with tempfile.TemporaryDirectory(prefix="douyin-compression-http-") as directory:
    database = Path(directory) / "pipeline.db"
    http_port, ws_port = free_port(), free_port()
    while http_port == ws_port:
        ws_port = free_port()
    base = f"http://127.0.0.1:{http_port}"
    token = "isolated-compression-test"

    def request(path, body=None, expected=200, timeout=2):
        data = body if isinstance(body, bytes) else None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(base + path, data=data,
                                     headers={"Content-Type": "application/json", "X-Internal-Token": token})
        try:
            response = urllib.request.urlopen(req, timeout=timeout)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            payload = response.read()
            assert response.status == expected, f"{path}: expected {expected}, got {response.status}: {payload!r}"
            return json.loads(payload) if payload else None

    def until(check, description, seconds=5):
        deadline = time.monotonic() + seconds
        last_error = None
        while time.monotonic() < deadline:
            try:
                result = check()
                if result:
                    return result
            except OSError as error:
                last_error = error
            time.sleep(.02)
        raise AssertionError(f"Timed out waiting for {description}: {last_error}")

    def ready(child):
        assert child.poll() is None, "Backend exited before readiness"
        return request("/health/live", timeout=.25).get("status") == "ok"

    @contextmanager
    def backend(compression, startup_vacuum="off", wait=True, batch_rows="2", batch_pause="10"):
        environment = {**os.environ, "DATABASE_PATH": str(database), "COMPRESSION": compression,
                       "VACUUM_ON_STARTUP": startup_vacuum, "COMPRESSION_BATCH_ROWS": batch_rows,
                       "COMPRESSION_BATCH_PAUSE_MS": batch_pause, "BIND_HOST": "127.0.0.1",
                       "HTTP_PORT": str(http_port), "WS_PORT": str(ws_port), "INTERNAL_TOKEN": token}
        child = subprocess.Popen([str(Path(sys.argv[1]).resolve())], env=environment,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        try:
            if wait:
                until(lambda: ready(child), "backend readiness")
            yield child
        finally:
            if child.poll() is None:
                child.terminate()
            child.wait(timeout=5)

    def rows(sql):
        with closing(sqlite3.connect(database)) as connection:
            return connection.execute(sql).fetchall()

    def migrated():
        maintenance = request("/api/health")["maintenance"]
        scopes = {scope["scope"]: scope for scope in maintenance["reencode"]}
        return (all(scopes.get(scope, {}).get("done", False)
                    for scope in ("events", "event_details", "event_presentations"))
                and maintenance["vacuum_pending"])

    with backend("off"):
        request("/api/rooms", {"live_id": "111"}, expected=202)
        request("/internal/ingest", chat_envelope(1))
        original = rows("SELECT body FROM events ORDER BY id")[0][0]
        assert isinstance(original, str), "Compression off must create a legacy TEXT fixture"

    with backend("on"):
        until(migrated, "initial body migration")
        compressed = rows("SELECT body FROM events ORDER BY id")[0][0]
        assert isinstance(compressed, bytes) and gzip.decompress(compressed).decode() == original
        request("/api/maintenance/compact", {}, expected=202)

        def compacted():
            state = request("/api/maintenance/status")
            return not any(state[key] for key in ("vacuum_pending", "vacuum_running", "vacuum_queued"))

        until(compacted, "manual compaction after migration")
        assert request("/api/health")["events"] == 1, "Compaction must preserve recorded events"
        print("PASS compression HTTP: legacy migration preserves bytes and manual compaction is accepted", flush=True)

    with backend("off"):
        request("/internal/ingest", chat_envelope(2))
        original_second = rows("SELECT body FROM events ORDER BY id DESC LIMIT 1")[0][0]
        assert isinstance(original_second, str), "Off must still write TEXT after a completed migration"

    with backend("on"):
        until(migrated, "migration after an off/on cycle")
        compressed_second = rows("SELECT body FROM events ORDER BY id DESC LIMIT 1")[0][0]
        assert isinstance(compressed_second, bytes), "Re-enabling must migrate rows written after the previous completion"
        assert gzip.decompress(compressed_second).decode() == original_second
        print("PASS compression HTTP: completed migration survives an off/on cycle without skipping new rows", flush=True)

    # A writer lock would stop Store construction before HTTP starts. Pin a WAL
    # reader instead: constructor transactions and VACUUM can run, while the
    # final TRUNCATE checkpoint waits, keeping startup maintenance observable.
    with closing(sqlite3.connect(database, isolation_level=None)) as reader:
        reader.execute("PRAGMA wal_autocheckpoint=0")
        reader.execute("UPDATE rooms SET updated_at=updated_at+1 WHERE live_id='111'")
        reader.execute("BEGIN")
        reader.execute("SELECT updated_at FROM rooms WHERE live_id='111'").fetchone()
        with backend("on", startup_vacuum="on", wait=False) as child:
            try:
                until(lambda: request("/health/live", timeout=.25).get("status") == "maintenance",
                      "responsive startup maintenance", seconds=2)
                state = request("/api/maintenance/status", timeout=.25)
                assert state["vacuum_running"], "Maintenance state must remain queryable during VACUUM"
                request("/api/rooms", expected=503, timeout=.25)
            finally:
                reader.execute("ROLLBACK")
            until(lambda: ready(child), "readiness after startup compaction")
            state = request("/api/maintenance/status")
            assert not state["vacuum_running"] and not state["vacuum_pending"]
            assert request("/api/health")["events"] == 2
            print("PASS compression HTTP: startup maintenance stays reachable and resumes after lock release", flush=True)

    for sequence, batch_rows in ((3, "0"), (4, "invalid")):
        with backend("on", batch_rows=batch_rows, batch_pause="invalid") as child:
            request("/internal/ingest", chat_envelope(sequence))
            assert ready(child), "Invalid maintenance settings must not terminate the service"
            assert request("/api/health")["events"] == sequence, "Collection must continue with fallback maintenance settings"
    print("PASS compression HTTP: zero and invalid batch settings fall back without stopping collection", flush=True)
