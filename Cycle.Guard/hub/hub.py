#!/usr/bin/env python3
"""
CycleGuard Hub — Raspberry Pi central service (v2 with sensor_fault support)

Threads:
  - Main: Flask web server (dashboard + API)
  - MQTT subscriber: listens for bike events, queues them
  - SQLite writer: drains the queue and persists to disk

Architecture deliberately decouples network ingestion from disk writes
via a queue so a slow disk write never blocks the MQTT thread.
"""

import json
import queue
import sqlite3
import threading
import time
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path

import paho.mqtt.client as mqtt
from flask import Flask, jsonify, render_template

# --- Config ---
MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC = "cycleguard/+/events"
DB_PATH = Path(__file__).parent / "cycleguard.db"
FLASK_HOST = "0.0.0.0"
FLASK_PORT = 5000

# --- Shared state ---
event_queue: queue.Queue = queue.Queue()
stats = {
    "received": 0,
    "stored": 0,
    "last_event_at": None,
    "errors": 0,
}
stats_lock = threading.Lock()


@contextmanager
def get_db():
    conn = sqlite3.connect(DB_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    try:
        yield conn
    finally:
        conn.close()


def init_db() -> None:
    with get_db() as conn:
        conn.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                bike_id TEXT NOT NULL,
                event_type TEXT NOT NULL,
                peak_proximity INTEGER,
                impact_g REAL,
                latitude REAL,
                longitude REAL,
                has_gps_fix INTEGER NOT NULL,
                bike_timestamp_ms INTEGER,
                sensor TEXT,
                stuck_at INTEGER,
                received_at TEXT NOT NULL
            )
        """)
        # Migration: add new columns if upgrading from v1 schema
        cols = {row["name"] for row in conn.execute("PRAGMA table_info(events)")}
        if "sensor" not in cols:
            conn.execute("ALTER TABLE events ADD COLUMN sensor TEXT")
        if "stuck_at" not in cols:
            conn.execute("ALTER TABLE events ADD COLUMN stuck_at INTEGER")
        conn.commit()


def mqtt_thread() -> None:
    def on_connect(client, _userdata, _flags, rc, _props=None):
        if rc == 0:
            print(f"[mqtt] connected, subscribing to {MQTT_TOPIC}")
            client.subscribe(MQTT_TOPIC)
        else:
            print(f"[mqtt] connect failed rc={rc}")

    def on_message(_client, _userdata, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            print(f"[mqtt] bad payload: {exc}")
            with stats_lock:
                stats["errors"] += 1
            return

        parts = msg.topic.split("/")
        bike_id = parts[1] if len(parts) >= 2 else "unknown"

        event = {
            "bike_id": bike_id,
            "event_type": payload.get("type", "unknown"),
            "peak_proximity": payload.get("peak_prox"),
            "impact_g": payload.get("impact_g"),
            "latitude": payload.get("lat"),
            "longitude": payload.get("lng"),
            "has_gps_fix": bool(payload.get("has_gps", False)),
            "bike_timestamp_ms": payload.get("ts"),
            "sensor": payload.get("sensor"),
            "stuck_at": payload.get("stuck_at"),
        }
        event_queue.put(event)

        with stats_lock:
            stats["received"] += 1
            stats["last_event_at"] = datetime.now().isoformat(timespec="seconds")

        print(f"[mqtt] queued event from {bike_id}: {event['event_type']}")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_connect = on_connect
    client.on_message = on_message

    while True:
        try:
            client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
            client.loop_forever()
        except (ConnectionRefusedError, OSError) as exc:
            print(f"[mqtt] connection error: {exc}, retrying in 5s")
            time.sleep(5)


def writer_thread() -> None:
    while True:
        event = event_queue.get()
        try:
            with get_db() as conn:
                conn.execute("""
                    INSERT INTO events (
                        bike_id, event_type, peak_proximity, impact_g,
                        latitude, longitude, has_gps_fix,
                        bike_timestamp_ms, sensor, stuck_at, received_at
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    event["bike_id"],
                    event["event_type"],
                    event["peak_proximity"],
                    event["impact_g"],
                    event["latitude"],
                    event["longitude"],
                    1 if event["has_gps_fix"] else 0,
                    event["bike_timestamp_ms"],
                    event["sensor"],
                    event["stuck_at"],
                    datetime.now().isoformat(timespec="seconds"),
                ))
                conn.commit()
            with stats_lock:
                stats["stored"] += 1
            print(f"[writer] stored {event['event_type']} event")
        except sqlite3.Error as exc:
            print(f"[writer] db error: {exc}")
            with stats_lock:
                stats["errors"] += 1
        finally:
            event_queue.task_done()


app = Flask(__name__)


@app.route("/")
def dashboard():
    return render_template("dashboard.html")


@app.route("/api/events")
def api_events():
    with get_db() as conn:
        rows = conn.execute("""
            SELECT * FROM events ORDER BY id DESC LIMIT 100
        """).fetchall()
        return jsonify([dict(r) for r in rows])


@app.route("/api/stats")
def api_stats():
    with get_db() as conn:
        totals = conn.execute("""
            SELECT
                COUNT(*) AS total,
                SUM(CASE WHEN event_type='close_call' THEN 1 ELSE 0 END) AS close_calls,
                SUM(CASE WHEN event_type='impact' THEN 1 ELSE 0 END) AS impacts,
                SUM(CASE WHEN event_type='sensor_fault' THEN 1 ELSE 0 END) AS faults
            FROM events
        """).fetchone()
        # Latest sensor fault (if any) - shown as a banner on the dashboard
        latest_fault = conn.execute("""
            SELECT received_at, sensor, stuck_at, bike_id
            FROM events
            WHERE event_type='sensor_fault'
            ORDER BY id DESC LIMIT 1
        """).fetchone()
    with stats_lock:
        live = dict(stats)
    return jsonify({
        "totals": dict(totals),
        "queue_depth": event_queue.qsize(),
        "runtime": live,
        "latest_fault": dict(latest_fault) if latest_fault else None,
    })


def main() -> None:
    init_db()

    t1 = threading.Thread(target=mqtt_thread, name="mqtt", daemon=True)
    t2 = threading.Thread(target=writer_thread, name="writer", daemon=True)
    t1.start()
    t2.start()

    print(f"[hub] starting Flask on {FLASK_HOST}:{FLASK_PORT}")
    app.run(host=FLASK_HOST, port=FLASK_PORT, debug=False, use_reloader=False)


if __name__ == "__main__":
    main()