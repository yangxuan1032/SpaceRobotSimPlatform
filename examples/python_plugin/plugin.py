"""Example SRSP sidecar plugin (target shape of the `srsp` Python SDK).

The SDK itself ships with milestone M2. This file documents the API we are
building towards and is intentionally excluded from the build.
"""

from __future__ import annotations

import numpy as np

from srsp import CommandBatch, Context, Plugin


class GravityCompensatedPd(Plugin):
    """Joint-space PD plus gravity/bias compensation."""

    id = "org.srsp.examples.python_controller"
    capabilities = ["srsp.control/controller@1"]

    def on_configure(self, config: dict) -> None:
        self.kp = float(config.get("kp", 20.0))
        self.kd = float(config.get("kd", 4.0))
        self.q_des = np.asarray(config.get("q_des", []), dtype=np.float64)

    def on_reset(self, ctx: Context) -> None:
        ctx.state  # Arrow tables are decoded lazily and zero-copy
        self.iteration = 0

    def on_step(self, ctx: Context) -> CommandBatch:
        state = ctx.state
        q = np.asarray(state.joints["q"])
        qd = np.asarray(state.joints["qd"])

        if self.q_des.size == 0:
            self.q_des = q.copy()

        tau = self.kp * (self.q_des - q) - self.kd * qd
        tau += np.asarray(ctx.dynamics.bias_forces())  # gravity + Coriolis

        self.iteration += 1
        return CommandBatch(joint_torque=tau, valid_until=ctx.t + 0.1)


if __name__ == "__main__":
    GravityCompensatedPd.serve()
