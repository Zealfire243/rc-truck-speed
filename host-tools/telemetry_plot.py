"""
Live telemetry plot.

    uv run telemetry_plot.py --port /dev/tty.usbmodem1234
    uv run telemetry_plot.py --replay runs/2026-08-10-run7.jsonl

Panels, and why each one is there:

  RPM         per axle, plus commanded. The obvious one.
  iq / id     torque and flux current. **Watch id.** It should sit near zero;
              drift means the observer angle is wrong, and it is the earliest
              and clearest warning you get. See motor-ctrl/inc/foc.h.
  Speed       wheel-derived vs. GPS ground truth. The gap between them is
              wheelspin — that gap is the signal traction control gets tuned
              against in Phase 6.
  vbat / temp the two things that end runs.

Flags are printed rather than plotted; a critical flag stops the plot and says
so, because at that point you should be looking at the truck.

STUB — the matplotlib wiring is not written yet. The parsing and replay paths
in protocol.py are real and testable now.
"""

from __future__ import annotations

import argparse


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--port", help="serial port")
    p.add_argument("--replay", help="run log to replay instead")
    p.add_argument("--baud", type=int, default=921600)
    a = p.parse_args()

    if not a.port and not a.replay:
        p.error("need --port or --replay")

    # TODO: matplotlib animation over a rolling window, four subplots as above.
    raise SystemExit("not implemented yet -- see the module docstring")


if __name__ == "__main__":
    main()
