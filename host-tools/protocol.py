"""
Speed Truck Link Protocol v1 — Python reference implementation.

Mirrors `motor-ctrl/inc/protocol.h` and `supervisor/main/protocol.h`. When you
change docs/PROTOCOL.md, change all three and bump PROTO_VERSION.

Stdlib only. This gets copied to whatever machine is at the track.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Any, Iterator

PROTO_VERSION = 1
MAX_LINE = 512
LINK_BAUD = 921600

# Axle indexing — docs/PROTOCOL.md §2. Exactly 2 elements, always this order.
AXLE_FRONT = 0
AXLE_REAR = 1
AXLE_NAMES = ("F", "R")

FLAGS = (
    "estop",
    "rc_lost",
    "desync",
    "obs_lost",
    "oc_trip",
    "overtemp",
    "cap",
    "iq_clamp",
    "vbat_low",
    "overrun",
)

# Flags that mean stop what you are doing and look at the truck.
CRITICAL_FLAGS = frozenset({"estop", "desync", "oc_trip", "overtemp", "obs_lost"})

PARAMS = (
    "pid.kp", "pid.ki", "pid.kd", "pid.ff",
    "foc.kp_i", "foc.ki_i", "foc.id_ref",
    "cap.mph", "limit.iq", "limit.accel", "tire.profile",
    "split.bias", "tc.gain", "tc.deadband",
    "tlm.hz",
    "bringup.duty", "bringup.freq",
)


@dataclass
class Telemetry:
    """One `tlm` frame. Optional fields are None when absent — early phases
    genuinely don't have them, and treating a missing field as zero produces
    plots that look like data."""

    ts: int
    mode: str
    cmd: list[float]
    rpm: list[float]
    iq: list[float]
    id: list[float] = field(default_factory=list)
    vbat: float = 0.0
    temp: list[float] = field(default_factory=list)
    mph: float | None = None
    gps_mph: float | None = None
    gps_sats: int | None = None
    slip: float | None = None
    flags: list[str] = field(default_factory=list)

    # Which tires the firmware believes are fitted. Every mph figure above is
    # computed from this, so a log without it is a log you cannot trust.
    tire: str | None = None
    tire_od: float | None = None

    @property
    def has_critical_flag(self) -> bool:
        return bool(CRITICAL_FLAGS.intersection(self.flags))


def parse_line(line: str) -> dict[str, Any] | None:
    """Parse one wire line.

    Returns None for anything that isn't a frame. **Any line whose first
    non-whitespace character is not `{` is silently skipped** — log output
    shares the link during bring-up and that is not a framing error.
    See docs/PROTOCOL.md §1.1.
    """
    s = line.strip()
    if not s or s[0] != "{":
        return None
    if len(s) > MAX_LINE:
        return None
    try:
        obj = json.loads(s)
    except json.JSONDecodeError:
        return None
    if not isinstance(obj, dict) or "t" not in obj:
        return None
    return obj


def as_telemetry(obj: dict[str, Any]) -> Telemetry | None:
    """Convert a parsed frame to Telemetry, or None if it isn't one."""
    if obj.get("t") != "tlm":
        return None
    gps = obj.get("gps") or {}
    return Telemetry(
        ts=int(obj.get("ts", 0)),
        mode=str(obj.get("mode", "?")),
        cmd=list(obj.get("cmd", [])),
        rpm=list(obj.get("rpm", [])),
        iq=list(obj.get("iq", [])),
        id=list(obj.get("id", [])),
        vbat=float(obj.get("vbat", 0.0)),
        temp=list(obj.get("temp", [])),
        mph=obj.get("mph"),
        gps_mph=gps.get("mph"),
        gps_sats=gps.get("sats"),
        slip=obj.get("slip"),
        flags=list(obj.get("flags", [])),
        tire=obj.get("tire"),
        tire_od=obj.get("tire_od"),
    )


def replay(path: str) -> Iterator[Telemetry]:
    """Read a run log off the SD card. Same parser as the live link — that's
    the whole reason the log format is the wire format."""
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            obj = parse_line(line)
            if obj is None:
                continue
            tlm = as_telemetry(obj)
            if tlm is not None:
                yield tlm


# --- Command construction -------------------------------------------------

class CommandBuilder:
    """Builds commands with incrementing ids. Hosts SHOULD treat a missing ack
    within 500 ms as a failure — see docs/PROTOCOL.md §4."""

    def __init__(self) -> None:
        self._id = 0

    def _next(self) -> int:
        self._id = (self._id + 1) & 0xFFFF
        return self._id

    def _frame(self, **kw: Any) -> tuple[int, str]:
        cid = self._next()
        kw["id"] = cid
        return cid, json.dumps(kw, separators=(",", ":")) + "\n"

    def set(self, param: str, value: Any, axle: int | None = None):
        kw: dict[str, Any] = {"t": "set", "p": param, "v": value}
        if axle is not None:
            kw["a"] = axle
        return self._frame(**kw)

    def get(self, param: str, axle: int | None = None):
        kw: dict[str, Any] = {"t": "get", "p": param}
        if axle is not None:
            kw["a"] = axle
        return self._frame(**kw)

    def tire_profile(self, name: str):
        """Declare which tires are fitted. Rebinds the speed calculation and
        the cap ceiling together.

        The firmware cannot verify this — it is a human declaration, and a
        wrong one makes the speed cap fail open. Hosts should surface the
        active profile prominently rather than treating it as a normal param.
        """
        return self.set("tire.profile", name)

    def mode(self, value: str):
        return self._frame(t="mode", v=value)

    def save(self):
        return self._frame(t="save")

    def ping(self):
        return self._frame(t="ping")

    def estop(self, engaged: bool = True):
        # The only command the firmware is required to honour in every state.
        return self._frame(t="estop", v=engaged)
