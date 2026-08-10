"""
Serial transport. Shared by telemetry_plot.py and tune.py.

Two things it exists to get right:

  1. Reading lines without blocking the caller. A reader thread drains the port
     into a queue; consumers poll. A plot that stalls on serial IO drops frames
     exactly when the run gets interesting.

  2. Matching acks to commands. Every command carries an id and gets exactly
     one ack. A command that is silently ignored looks identical to one that
     worked, which during a tuning session is genuinely dangerous.
"""

from __future__ import annotations

import queue
import threading
import time
from typing import Any

import serial

import protocol


class SerialLink:
    def __init__(self, port: str, baud: int = protocol.LINK_BAUD, timeout: float = 0.1):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.frames: queue.Queue[dict[str, Any]] = queue.Queue(maxsize=4096)
        self.cmd = protocol.CommandBuilder()
        self._acks: dict[int, dict[str, Any]] = {}
        self._acks_lock = threading.Lock()
        self._stop = threading.Event()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self.ser.readline()
            except serial.SerialException:
                break
            if not raw:
                continue
            obj = protocol.parse_line(raw.decode("utf-8", errors="replace"))
            if obj is None:
                continue
            if obj.get("t") in ("ack", "gains"):
                with self._acks_lock:
                    self._acks[int(obj.get("id", -1))] = obj
            try:
                self.frames.put_nowait(obj)
            except queue.Full:
                # Drop the oldest rather than the newest — during a run the
                # most recent frame is always the one that matters.
                try:
                    self.frames.get_nowait()
                    self.frames.put_nowait(obj)
                except queue.Empty:
                    pass

    def send(self, built: tuple[int, str], wait_ack: float = 0.5) -> dict[str, Any] | None:
        """Send a command and wait for its ack. Returns None on timeout —
        treat that as a failure, not as success."""
        cid, line = built
        self.ser.write(line.encode())
        if wait_ack <= 0:
            return None
        deadline = time.monotonic() + wait_ack
        while time.monotonic() < deadline:
            with self._acks_lock:
                if cid in self._acks:
                    return self._acks.pop(cid)
            time.sleep(0.005)
        return None

    def estop(self) -> None:
        """Send estop and do not wait politely. The firmware is required to
        honour this in every state."""
        _, line = self.cmd.estop(True)
        self.ser.write(line.encode())
        self.ser.flush()

    def close(self) -> None:
        self._stop.set()
        self._reader.join(timeout=1.0)
        self.ser.close()
