"""Independent oracle for the one-joint model, computed with MuJoCo.

Mirrors the SRM model used by test_aba_isolation: a 10 kg base with identity
inertia and a 2 kg link whose COM sits 0.3 m along z, hinged about z at the
base origin. Prints M and the bias force so they can be compared with
mass_matrix() / bias_forces() of the kernel.
"""
import json
import numpy as np
import mujoco

XML = """
<mujoco>
  <worldbody>
    <body name="base" pos="0 0 0">
      <freejoint/>
      <inertial pos="0 0 0" mass="10" diaginertia="1 1 1"/>
      <body name="link" pos="0 0 0">
        <joint name="j1" type="hinge" axis="0 0 1"/>
        <inertial pos="0 0 0.3" mass="2" diaginertia="0.2 0.2 0.05"/>
      </body>
    </body>
  </worldbody>
</mujoco>
"""

model = mujoco.MjModel.from_xml_string(XML)
data = mujoco.MjData(model)

# qpos: free joint (3 pos, 4 quat wxyz) + hinge; qvel: 3 lin, 3 ang, 1 hinge
rng = np.random.default_rng(11)
data.qpos[:] = 0.0
data.qpos[3] = 1.0  # quaternion w
data.qpos[7] = 0.3
data.qvel[:] = [0.1, -0.2, 0.05, 0.3, -0.4, 0.2, 0.5]
mujoco.mj_forward(model, data)

M = np.zeros((model.nv, model.nv))
mujoco.mj_fullM(model, data, M)
bias = data.qfrc_bias.copy()

print("nq", model.nq, "nv", model.nv)
print("M diag", np.array2string(np.diag(M), precision=6))
print("bias", np.array2string(bias, precision=6))