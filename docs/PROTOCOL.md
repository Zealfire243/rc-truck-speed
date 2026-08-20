# Speed Truck Link Protocol v1

The wire format shared by three codebases. Deliberately the same shape as the crawler's protocol —
JSON lines, `t`-tagged frames, `id`-echoed commands — so `host-tools/` ports across with the field
names changed and nothing else.

| Side | Role | Implementation |
|---|---|---|
| Motor controller **×2** | producer of `tlm`/`evt`/`ack`, consumer of commands | [`motor-ctrl/inc/protocol.h`](../motor-ctrl/inc/protocol.h), [`link.c`](../motor-ctrl/src/link.c) |
| ESP32 supervisor | relay + consumer + GPS injector | [`supervisor/main/protocol.h`](../supervisor/main/protocol.h) |
| Host toolkit | consumer + tuner | [`host-tools/protocol.py`](../host-tools/protocol.py) |

> ⚠ **There are now two producers, not one.** The architecture moved from one dual-channel G474 to
> **two independent controllers, one per axle** — see
> [ARCHITECTURE.md](ARCHITECTURE.md#why-two-controllers-instead-of-one). This document still
> describes the single-producer v1 wire format, and **§2's two-element axle arrays are the part
> that no longer fits**: each controller knows only its own axle.
>
> **This needs a v2 and it is not designed yet.** The open question is whether each controller
> emits single-axle frames that the ESP32 merges into the existing two-element shape (keeps
> `host-tools/` unchanged, makes the ESP32 a stitcher), or whether frames carry an explicit
> `axle` field and consumers do the merging (honest, but every consumer changes). Decide it at
> [Phase 3](BUILD_PHASES.md#phase-3--both-channels) with both links real. Until then Phases 1–2b
> are single-controller and v1 is correct as written.

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
| **Controller ↔ ESP32 UART** | The internal link. `921600`, 8N1, no flow control. Common ground required. **One per controller** — two links, or one CAN bus. |
| **Controller ↔ host USB serial** | Direct connection for bench work in Phases 1–2b, before the ESP32 exists. `921600`. |
| **ESP32 WiFi (TCP)** | Port **3737**, one client. The ESP32 relays controller frames verbatim and adds its own. |
| **ESP32 WiFi (UDP)** | Port **3738**, lossy telemetry-only for high-rate plotting. Commands are never accepted over UDP. |

**CAN is now the preferred transport, not merely an upgrade path**, because two controllers plus
the ESP32 on one bus beats three point-to-point UARTs — and the controllers need a peer link to
each other regardless, for the desync interlock. The F405 has bxCAN natively; the ESP32's TWAI
peripheral speaks classic CAN. See
[ARCHITECTURE.md](ARCHITECTURE.md#the-desync-interlock-across-two-controllers) and
[BUILD_PHASES.md § Open items](BUILD_PHASES.md#open-items).

> **The peer link is not this protocol.** Heartbeat and cross-axle RPM/current between the two
> controllers is a safety path with a hard latency budget; JSON lines are the wrong tool. Keep it
> binary, fixed-length, and separate. And the *cut* is a hardware fault line, not a message at
> all.

---

## 1.3 The two binary buses — NOT this protocol

Two links on this vehicle carry control-loop inputs with hard latency budgets. **They are not JSON
lines and must never become them.** Documented here because this is where people will look, not
because they share a format.

| Bus | From → to | Rate | Payload |
|---|---|---|---|
| **Peer frame** | controller ↔ controller | 500–1000 Hz | 8 bytes, below |
| **Advisory frame** | ESP32 → both controllers | 200 Hz | 8 bytes, below |

Both fit one classic CAN frame exactly. On a 1 Mbit bus an 8-byte frame is ~130 µs on the wire, so
two controllers at 1 kHz plus the supervisor at 200 Hz is roughly 29% bus load with worst-case
latency around 260 µs — an order of magnitude inside a 2 ms TC period. See
[ARCHITECTURE.md](ARCHITECTURE.md#traction-control-across-two-axles).

### Peer frame — controller ↔ controller

```
  wheel_speed    int16   0.01 m/s    this axle's wheel speed
  iq             int16   0.01 A      q-axis current
  motor_temp     int8    °C
  state_flags    uint8   running | faulted | observer_locked | sensored
  seq            uint8   wraps; used for staleness detection
  reserved       uint8
```

Carries the *data* half of the desync interlock. The *cut* half is a hardware fault line, not a
message — see
[ARCHITECTURE.md](ARCHITECTURE.md#the-desync-interlock-across-two-controllers).

### Advisory frame — ESP32 → both controllers

```
  v_ground_est   int16   0.01 m/s    IMU-integrated, GPS-corrected
  a_x            int16   0.01 m/s²   raw, so controllers can sanity-check
  yaw_rate       int16   0.01 rad/s
  flags          uint8   gps_lock | imu_ok | confidence
  seq            uint8   wraps; staleness detection
```

> ⚠ **This frame can only ever *reduce* torque.** It enters the controllers as one term in a
> `min()` and there is no code path by which it raises a limit. Stale `seq` or cleared flags mean
> the term is **skipped entirely** — not clamped, not extrapolated. See
> [the supervisor exception](ARCHITECTURE.md#the-supervisor-exception--advisory-input-that-can-only-subtract).

> **Steering angle is deliberately absent.** It is already in the CRSF frame both controllers
> decode; routing it through the supervisor would add latency and a dependency for nothing.

---

## 2. Axle indexing

Every array of per-axle values is **exactly 2 elements**, always in this order:

| Index | Axle | Short | Owned by |
|---|---|---|---|
| 0 | Front | `F` | Front controller, its TIM1 |
| 1 | Rear | `R` | Rear controller, its TIM1 |

⚠ **Both controllers use TIM1.** An earlier version of this table assigned the rear axle to TIM8,
which was correct when one chip drove both. Each controller now has its own TIM1 and its own ADCs.

Sign convention: **positive = forward**, always, regardless of how the motor phases are wired.
Correcting for a reversed motor is the firmware's job (`pin_map.h` → `MOTOR_INVERT_*`), not the
consumer's.

---

## 3. Device → Host frames

### 3.1 `hello` — sent once on boot

```json
{"t":"hello","proto":1,"fw":"0.1.0","app":"bringup","axles":2,"hz":50,"cap_mph":20.0,"tire":"street","tire_od":55.0,"tire_w":22.0}
```

| Field | Type | Meaning |
|---|---|---|
| `proto` | int | Protocol version. This document is `1`. |
| `fw` | string | Firmware semver. |
| `app` | string | `"bringup"` or `"full"` — which build is running. |
| `axles` | int | Axles reporting real data. `1` in Phases 1–2, `2` from Phase 3. |
| `hz` | int | Nominal telemetry rate. |
| `cap_mph` | float | **The speed cap in force.** A host MUST display this prominently. |
| `tire` | string | Active tire profile — `"street"` (55×22), `"test"` (70×25), `"wide"` (70×35), or `"record"` (70×25, high-rated). A host MUST display this prominently too; see the warning below. |
| `tire_od` | float | Tire OD in mm for the active profile. Every speed figure in `tlm` is computed from it. |
| `tire_w` | float | Tire width in mm. Does **not** affect any speed figure — it is recorded so a log identifies which tires produced it, which is what makes per-tire μ measurements attributable. |

> **Why `tire` is displayed as loudly as `cap_mph`.** Tire OD feeds the speed
> calculation, which feeds the cap. If 70 mm tires are fitted while the
> firmware believes 55 mm, the truck computes 55 mph while actually doing 70 —
> **the cap fails open.** The firmware cannot detect what is bolted on, so the
> profile is a declaration by a human, and the display is what makes a wrong
> declaration visible before the run rather than after.

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
| `cap.mph` | float | > 0 | **The speed cap.** Clamped to the *lower* of the compiled phase-gate ceiling and the active tire profile's ceiling — a `set` above that is refused, so neither a host bug nor a tire swap can raise it. |
| `limit.iq` | float | > 0 | Torque current ceiling, amps per axle. |
| `limit.accel` | float | > 0 | Setpoint slew limit, RPM/s. Launch traction control lives here. |
| `split.bias` | float | -1…1 | Front/rear torque split. `0` even, `-1` all front, `+1` all rear. |
| `tc.gain` | float | ≥ 0 | Traction control authority. `0` disables. |
| `tc.deadband` | float | ≥ 0 | Slip ratio below which traction control does nothing. |
| `tire.profile` | string | `street` \| `test` \| `wide` \| `record` | **Which tires are fitted.** Rebinds the speed calculation and the cap ceiling together so they cannot be mismatched. Each profile's ceiling is **computed** from its tire's rated RPM times a derate, not declared — so fitting a higher-rated tire raises the cap and a cheap one lowers it, with no code change and no way for the two to disagree. `test` and `wide` share an OD and a rating, so they differ only in grip. Selecting a lower profile immediately pulls `cap.mph` down if it was above the new ceiling; selecting a higher one never auto-raises it. |
| `tlm.hz` | int | 1…200 | Telemetry rate. |
| `bringup.duty` | float | -1…1 | **Phase 1 app only.** Open-loop duty. Still subject to `limit.iq`. |
| `bringup.freq` | float | ≥ 0 | **Phase 1 app only.** Forced commutation electrical frequency, Hz. |

Parameters are **live** — they take effect on the next control tick — and are *not* persisted until
a `save`.

**`cap.mph` is special.** It is the one parameter whose whole job is to be hard to raise. Two
independent ceilings bound it and both must permit a speed:

- the **compiled phase-gate ceiling**, which moves one gate at a time, in source, in a commit — how
  far the *truck* has been proven;
- the **active tire profile's ceiling** — what the *tires* are rated for.

Neither overrides the other. A tuning session cannot talk the truck into going faster than the
phase it has passed, and fitting slower tires cannot leave a fast cap behind.

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
