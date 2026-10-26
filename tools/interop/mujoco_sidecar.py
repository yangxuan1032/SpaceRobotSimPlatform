#!/usr/bin/env python3
"""MuJoCo sidecar prototype for W7 interoperability.

Usage: python tools/interop/mujoco_sidecar.py examples/models/arm6.mjcf.xml

This is the prototype of the out-of-process MuJoCo backend described in
docs/07 W7. It loads an MJCF model, steps it once and prints the resulting
state as JSON on stdout, which is the shape the real sidecar will stream over
gRPC. When `mujoco` is not installed the script prints SKIP and exits 0.
"""

from __future__ import annotations

import json
import sys


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: mujoco_sidecar.py <model.mjcf.xml>")
        return 2
    try:
        import mujoco
    except Exception as exc:  # noqa: BLE001 - optional dependency
        print(f"SKIP: mujoco is not available ({exc})")
        return 0

    model = mujoco.MjModel.from_xml_path(argv[1])
    data = mujoco.MjData(model)
    mujoco.mj_step(model, data)
    payload = {
        "nq": int(model.nq),
        "nv": int(model.nv),
        "time": float(data.time),
        "qpos": [float(value) for value in data.qpos],
        "qvel": [float(value) for value in data.qvel],
    }
    print(json.dumps(payload))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
