# Speed Truck Link Protocol v1

The wire format shared by three codebases. Deliberately the same shape as the crawler's protocol —
JSON lines, `t`-tagged frames, `id`-echoed commands — so `host-tools/` ports across with the field
names changed and nothing else.

| Side | Role | Implementation |
|---|---|---|
| STM32G4 | producer of `tlm`/`evt`/`ack`, consumer of commands | [`motor-ctrl/inc/protocol.h`](../motor-ctrl/inc/protocol.h), [`link.c`](../motor-ctrl/src/link.c) |
| ESP32 supervisor | relay + consumer + GPS injector | [`supervisor/main/protocol.h`](../supervisor/main/protocol.h) |
| Host toolkit | consumer + tuner | [`host-tools/protocol.py`](../host-tools/protocol.py) |

---

## 1. Framing

- **One JSON object per line**, UTF-8, terminated by `\n` (`\r\n` also accepted on read).
- **No embedded newlines.** No pretty-printing.
- **Max line length 512 bytes.** Longer lines are truncated by the sender and MUST be dropped by
  the receiver.
- **Every frame has a `t` (type) field.**
- Field order is not significant. Unknown fields MUST be ignored.

### 1.1 The non-JSON rule

Any received line whose first non-whitespace character is not `{` MUST be silently skipped. Log
output shares the link during bring-up, and this keeps a stray print from being a framing error.
All reference parsers do this.

### 1.2 Transports

| Transport | Notes |
|---|---|
| **G4 ↔ ESP32 UART** | The internal link. `921600`, 8N1, no flow control. Common ground required. |
| **G4 ↔ host USB serial** | Direct connection for bench work in Phases 1–3, before the ESP32 exists. `921600`. |
| **ESP32 WiFi (TCP)** | Port **3737**, one client. The ESP32 relays G4 frames verbatim and adds its own. |
| **ESP32 WiFi (UDP)** | Port **3738**, lossy telemetry-only for high-rate plotting. Commands are never accepted over UDP. |

FDCAN between the G4 and ESP32 is the upgrade path if UART framing gets tight — see
[BUILD_PHASES.md § Open items](BUILD_PHASES.md#open-items).

---

## 2. Axle indexing

Every array of per-axle values is **exactly 2 elements**, always in this order:

| Index | Axle | Short | Timer |
|---|---|---|---|
| 0 | Front | `F` | TIM1 |
| 1 | Rear | `R` | TIM8 |

Sign convention: **positive = forward**, always, regardless of how the motor phases are wired.
Correcting for a reversed motor is the firmware's job (`pin_map.h` → `MOTOR_INVERT_*`), not the
consumer's.

---

## 3. Device → Host frames

### 3.1 `hello` — sent once on boot

```json
{"t":"hello","proto":1,"fw":"0.1.0","app":"bringup","axles":2,"hz":50,"cap_mph":20.0}
```

| Field | Type | Meaning |
|---|---|---|
| `proto` | int | Protocol version. This document is `1`. |
| `fw` | string | Firmware semver. |
| `app` | string | `"bringup"` or `"full"` — which build is running. |
| `axles` | int | Axles reporting real data. `1` in Phases 1–2, `2` from Phase 3. |
| `hz` | int | Nominal telemetry rate. |
| `cap_mph` | float | **The compiled-in speed cap.** A host MUST display this prominently. |

A host that sees an unknown `proto` MUST warn and continue read-only.

### 3.2 `tlm` — the telemetry frame

```json
{"t":"tlm","ts":128340,"mode":"run","cmd":[0.42,0.42],"rpm":[8420.0,8455.0],"iq":[12.4,12.8],"id":[0.1,0.0],"vbat":14.9,"temp":[48.2,51.0],"mph":63.4,"gps":{"mph":61.9,"sats":9},"slip":0.024,"flags":[]}
```

| Field | Type | Meaning |
|---|---|---|
| `ts` | int | Milliseconds since boot. Monotonic. |
| `mode` | string | `"idle"` \| `"run"` \| `"fault"` |
| `cmd` | float[2] | Commanded torque per axle, `-1.0 … +1.0`. Open loop: this is duty. |
| `rpm` | float[2] | **Motor** RPM per axle, from the observer. Divide by `final_drive` for wheel RPM. |
| `iq` | float[2] | Torque-producing current, amps. The one that matters. |
| `id` | float[2] | Flux current, amps. Should sit near zero — drift here means the Park transform or the observer angle is off. |
| `vbat` | float | Pack voltage under load. Watch this sag on a full-power run. |
| `temp` | float[2] | Inverter or motor temperature, °C. |
| `mph` | float | Wheel-derived ground speed. **Overstates under wheelspin.** |
| `gps` | object | Optional, ESP32-injected. `mph` ground truth, `sats` count. Absent before Phase 5. |
| `slip` | float | Optional. `(wheel - ground) / wheel`. Absent without a GPS fix. |
| `flags` | string[] | Active condition flags, empty when healthy. See §5. |

`cmd`, `rpm`, `iq`, `vbat` are **required**; the rest are optional and may be absent in early phases.

> **Open-loop caveat.** In the Phase 1 bring-up app there is no RPM setpoint — the commanded
> quantity *is* duty, and `cmd` carries `-1…1` duty rather than a torque request.
> `hello.app == "bringup"` tells a client to expect this.

### 3.3 `evt` — asynchronous event

```json
{"t":"evt","ts":128400,"lvl":"warn","msg":"axle desync 412 rpm, both channels cut"}
```

`lvl` is `"info"` | `"warn"` | `"err"`.

### 3.4 `ack` — response to any command

```json
{"t":"ack","id":7,"ok":true}
{"t":"ack","id":8,"ok":false,"err":"cap.mph above compiled ceiling"}
```

`id` echoes the command's `id`. Every inbound command gets exactly one `ack`.

### 3.5 `gains` — response to `get` on a `pid.*` param

```json
{"t":"gains","id":9,"a":0,"kp":0.85,"ki":0.12,"kd":0.0,"ff":0.004}
```

`a` is the axle index.

---

## 4. Host → Device frames

Every command carries an `id` (uint16, host-assigned, incrementing). Hosts SHOULD treat a missing
`ack` within 500 ms as a failure.

### 4.1 `set` / `get`

```json
{"t":"set","id":1,"p":"pid.kp","v":0.85,"a":0}
{"t":"get","id":2,"p":"pid.kp","a":0}
```

- `p` — parameter name, see §4.4.
- `v` — value.
- `a` — optional axle index `0..1`. **Omit to apply to both.**

### 4.2 `mode`

```json
{"t":"mode","id":3,"v":"run"}
```

`v` is `"idle"` | `"run"`. The device may refuse if a fault is latched.

### 4.3 `save`, `ping`, `estop`

```json
{"t":"save","id":5}
{"t":"ping","id":6}
{"t":"estop","id":7,"v":true}
```

`estop` `true` cuts both channels and latches `mode:"fault"`; `false` clears the latch.
**`estop` is the only command the firmware is required to honour in every state.**

### 4.4 Parameter namespace

| Parameter | Type | Range | Meaning |
|---|---|---|---|
| `pid.kp` | float | ≥ 0 | Velocity loop proportional gain, per axle. |
| `pid.ki` | float | ≥ 0 | Velocity loop integral gain (per second). |
| `pid.kd` | float | ≥ 0 | Velocity loop derivative gain. |
| `pid.ff` | float | ≥ 0 | Feed-forward, torque per RPM of setpoint. |
| `foc.kp_i` | float | ≥ 0 | **Current loop** proportional gain. Distinct from `pid.kp` — different loop, 10× the rate. |
| `foc.ki_i` | float | ≥ 0 | Current loop integral gain. |
| `foc.id_ref` | float | — | Flux current reference. Zero for everything below base speed. Negative is field weakening; do not touch before Phase 8. |
| `cap.mph` | float | > 0 | **The speed cap.** Clamped to a compiled-in ceiling — a `set` above it is refused, so a host bug can't raise it. |
| `limit.iq` | float | > 0 | Torque current ceiling, amps per axle. |
| `limit.accel` | float | > 0 | Setpoint slew limit, RPM/s. Launch traction control lives here. |
| `split.bias` | float | -1…1 | Front/rear torque split. `0` even, `-1` all front, `+1` all rear. |
| `tc.gain` | float | ≥ 0 | Traction control authority. `0` disables. |
| `tc.deadband` | float | ≥ 0 | Slip ratio below which traction control does nothing. |
| `tlm.hz` | int | 1…200 | Telemetry rate. |
| `bringup.duty` | float | -1…1 | **Phase 1 app only.** Open-loop duty. Still subject to `limit.iq`. |
| `bringup.freq` | float | ≥ 0 | **Phase 1 app only.** Forced commutation electrical frequency, Hz. |

Parameters are **live** — they take effect on the next control tick — and are *not* persisted until
a `save`.

**`cap.mph` is special.** It is the one parameter whose whole job is to be hard to raise. The
compiled ceiling goes up one phase gate at a time, in source, in a commit. A tuning session cannot
talk the truck into going faster than the phase it has passed.

---

## 5. Condition flags

| Flag | Meaning |
|---|---|
| `estop` | Emergency stop latched. |
| `rc_lost` | No valid CRSF frame within the failsafe window. Torque zeroed, coasting. |
| `desync` | Axle speeds diverged beyond threshold. **Both** channels cut. |
| `obs_lost` | Rotor observer lost tracking on an axle. |
| `oc_trip` | Hardware overcurrent break fired. |
| `overtemp` | Thermal fold-back active. |
| `cap` | Speed cap is actively limiting. Expected during phase runs, not a fault. |
| `iq_clamp` | Torque current saturated at `limit.iq`. |
| `vbat_low` | Pack voltage below threshold under load. |
| `overrun` | Control loop missed its deadline. |

---

## 6. Keeping the three in sync

`motor-ctrl/inc/protocol.h` and `supervisor/main/protocol.h` carry the same constants as C
`#define`s; `host-tools/protocol.py` mirrors them in Python. When you change this document, change
all three and bump `PROTO_VERSION`.

### Changing the protocol

- **Adding an optional field** to `tlm` — no version bump. Parsers ignore unknowns.
- **Adding a new frame type** — no version bump. Unknown `t` values are ignored with a warning.
- **Changing or removing an existing field's meaning** — bump `PROTO_VERSION`.
