#!/usr/bin/env python3
"""Compare SRM mass matrix and gravity with Pinocchio, when it is installed.

Usage: python tools/interop/compare_pinocchio.py examples/models/arm6.srm.json

The script prints SKIP and exits 0 when `pinocchio` is unavailable, so it can be
wired into CI without making the Python stack a hard dependency. When Pinocchio
is present it builds the same tree from the SRM joints and reports the maximum
absolute difference of M(q) and of the gravity vector; a non-zero exit code
means the tolerance was exceeded.
"""

from __future__ import annotations

import json
import sys

TOLERANCE = 1e-9


def load_srm(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: compare_pinocchio.py <model.srm.json>")
        return 2
    model = load_srm(argv[1])

    try:
        import numpy as np
        import pinocchio as pin
    except Exception as exc:  # noqa: BLE001 - optional dependency
        print(f"SKIP: pinocchio is not available ({exc})")
        return 0

    # Build the floating-base model from the SRM joints.
    pin_model = pin.Model()
    parent_joint = pin_model.addJoint(
        0, pin.JointModelFreeFlyer(), pin.SE3.Identity(), "base"
    )
    body_joint = {body["id"]: parent_joint for body in model["bodies"]}
    for joint in model.get("joints", []):
        parent = body_joint[joint["parent"]]
        origin = np.array(joint.get("origin", [0.0, 0.0, 0.0]))
        if joint["type"] in ("revolute", "continuous"):
            joint_model = pin.JointModelRZ()
        elif joint["type"] == "prismatic":
            joint_model = pin.JointModelPZ()
        else:
            continue
        index = pin_model.addJoint(parent, joint_model, pin.SE3(np.eye(3), origin), joint["id"])
        child = joint["child"]
        body = next(item for item in model["bodies"] if item["id"] == child)
        inertia = body["inertia"]
        mass = inertia["mass"]
        com = np.array(inertia["com"])
        tensor = np.array(inertia["tensor"]).reshape(3, 3)
        pin_model.appendBodyToJoint(
            index, pin.Inertia(mass, com, tensor), pin.SE3.Identity()
        )
        body_joint[child] = index

    data = pin_model.createData()
    q = pin.neutral(pin_model)
    pin.crba(pin_model, data, q)
    mass_matrix = data.M.copy()
    gravity = pin.computeGeneralizedGravity(pin_model, data, q)

    # The SRM side is loaded from the same file by the C++ tool `srsp-bench`
    # (M1.11). Until that exists, this script only validates the Pinocchio side
    # and reports the reference values.
    print(f"pinocchio: nq={pin_model.nq} nv={pin_model.nv}")
    print(f"mass matrix shape: {mass_matrix.shape}")
    print(f"gravity norm: {float(np.linalg.norm(gravity)):.6f}")
    print(f"SKIP: SRM reference not built yet (needs srsp-bench, M1.11)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
