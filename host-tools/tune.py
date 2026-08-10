"""
Interactive tuner.

    uv run tune.py --port /dev/tty.usbmodem1234

Type parameter names and values; `save` persists. Run it next to
telemetry_plot.py — tuning without watching the response is guessing.

============================================================================
TUNING ORDER THAT WORKS
============================================================================

There are two nested loops and they must be tuned inner-first. Tuning the
velocity loop while the current loop is wrong produces gains that are
compensating for the wrong thing, and they stop working the moment you fix it.

  1. CURRENT LOOP (foc.kp_i, foc.ki_i) — Phase 2, motor on the bench.
     Command a step in iq and watch it. Raise kp_i until the response is fast
     with a little overshoot, then ki_i until steady-state error is gone. This
     loop runs at 20 kHz, so it should settle in a millisecond or two.

     Sanity check while you do it: id should stay near zero throughout. If it
     moves when you step iq, the observer angle is wrong and no amount of gain
     tuning will fix that.

  2. VELOCITY LOOP (pid.ff, pid.kp, pid.ki) — after the current loop is solid.
     ff first: it's the feed-forward term, and getting it right means the PI
     terms only have to correct the residual. Then kp, then ki. Leave kd at
     zero; derivative on a noisy speed estimate at 30,000 rpm is a bad trade.

  3. POLICY (limit.accel, tc.gain, split.bias) — Phase 6, on the ground,
     against the GPS slip data. Not on the bench; these are about tires.

============================================================================
SAFETY
============================================================================

`e` sends estop immediately without waiting for an ack. Know where that key is
before you start. `cap.mph` above the compiled ceiling will be refused — that
is intentional and it is not a bug to work around.

STUB — the REPL is not written yet.
"""

from __future__ import annotations

import argparse


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=921600)
    p.parse_args()

    # TODO: readline REPL over SerialLink. Commands: <param> <value> [axle],
    # `get <param>`, `save`, `e` for estop, `q` to quit.
    raise SystemExit("not implemented yet -- see the module docstring")


if __name__ == "__main__":
    main()
