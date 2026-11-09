# 组件标准与扩展规范

- 状态：生效（2026-11-09）
- 关联：[02 插件体系](02-plugin-contract.md)、[03 数据契约](03-data-contracts.md)、[11 模块总览](11-module-overview.md)、[01 架构](01-architecture.md)

## 1. 组件是什么

> 组件 = **清单（manifest）+ 类型化参数 schema + 生命周期 + 数据契约 + 能力实现**。
> 五者齐全才是标准组件；只有代码没有清单的是内部模块。

| 概念 | 含义 | 唯一性 |
|---|---|---|
| ComponentKind | 一类组件的 schema 与语义（如 `sensor.camera`） | 注册表内唯一 |
| ComponentId | 组件类型标识（如 `org.srsp.sensors.camera`） | 全局唯一 |
| ComponentInstance | 带参数的实例，`entity/<name>` | 实例内唯一 |
| Capability | 能力契约（如 `srsp.sensor/device@1`） | 版本化管理 |
| Implementation | 插件或内置实现 | 按 `(capability, version)` 解析 |

## 2. 组件类型（首批）

| kind | 能力域 | 动力学贡献 | 关键参数 | 数据契约 |
|---|---|---|---|---|
| `env.celestial` | `srsp.env/gravity@1` | 无（环境量） | `mu, radius_m, j2, ephemeris` | `srsp.env.state/1` |
| `dynamics.body` | `srsp.dynamics/body@1` | 新增体与质量 | `mass_kg, inertia, com_m, geometry` | `srsp.state.body/1` |
| `dynamics.joint` | `srsp.dynamics/joint@1` | 新增 DOF | `type, axis, lower, upper` | — |
| `dynamics.force` | `srsp.dynamics/force@1` | 力/力矩 | `model, params` | `srsp.force/1` |
| `dynamics.actuator` | `srsp.dynamics/actuator@1` | 力/力矩 | `type, axis, max_effort` | `srsp.actuator/1` |
| `dynamics.propulsion` | `srsp.dynamics/propulsion@1` | 质量时变 | `isp_s, propellant_kg` | `srsp.propulsion/1` |
| `sensor.camera` | `srsp.sensor/device@1` | 无质量或子体 | `width, height, fov_deg` | `srsp.sensor.image/1` |
| `sensor.lidar` | 同上 | 无质量或子体 | `rings, azimuth_samples, range_m` | `srsp.sensor.pointcloud/1` |
| `sensor.radar` | 同上 | 无质量或子体 | `freq_hz, bandwidth_hz, prf_hz` | `srsp.sensor.detection/1` |
| `control.controller` | `srsp.control/controller@1` | 无 | `law, gains, rate_hz` | `srsp.control.command/1` |
| `visual.renderer` | `srsp.render/renderer@1` | 无 | `backend, resolution` | `srsp.render.frame/1` |

## 3. 清单文件

`component.json`（与实现同目录）：

```json
{
  "schema": "srsp.component/1",
  "kind": "sensor.camera",
  "id": "org.srsp.sensors.camera",
  "version": "1.0.0",
  "capabilities": ["srsp.sensor/device@1"],
  "params": { "type": "object", "required": ["width", "height", "fov_deg"], "properties": {} },
  "mount": { "kind": "fixed", "default_mass_kg": 0.0 },
  "data": { "sample": { "request": "srsp.sensor.sample/1", "result": "srsp.sensor.image/1" } },
  "checkpoint": true
}
```

规则：

1. 必填参数缺失即拒绝装载，不在运行期补默认值；
2. 参数名带单位后缀（`_m/_kg/_s/_rad_s/_deg`），一律 SI；
3. `visual.*`（贴图/材质/网格）与动力学参数分文件，按 `entity` 关联；
4. SemVer 管理版本，能力主版本变化才允许破坏性调整；
5. 清单由 `docs_check` 校验 JSON 合法，能力名须能在 `docs/02` 的能力表中找到。

## 4. 生命周期

```
declare → validate → resolve → instantiate → init → tick|sample → checkpoint/restore → shutdown
```

| 阶段 | 失败语义 | 诊断内容 |
|---|---|---|
| validate | 拒绝装载 | 字段名、期望值、参数来源 |
| resolve | 拒绝装载 | 缺失宿主、挂载成环、frame 不存在 |
| instantiate | 拒绝装载 | ID 冲突、资源不足 |
| init | 拒绝装载 | 能力不可用（headless 相机 → `NOT_READY`） |
| tick/sample | 该步失败且不提交半状态 | entity、方法、步号、remidy |
| checkpoint/restore | 显式报错 | 版本/长度不匹配 |
| shutdown | 幂等 | 已释放则忽略 |

## 5. 扩展规则

| 想加什么 | 做法 | 内核改动 |
|---|---|---|
| 新组件类型 | 新清单 + 实现 + 测试，注册 kind 与能力 | 无 |
| 同类型新实例 | 新参数 + 挂载，`entity/<name>` | 无 |
| 新后端/算法 | 策略枚举新增值（默认 + 保留）或能力实现注册 | 仅枚举 |
| 新力模型 | `ForceRegistry` 注册条目 + 参数 | 无 |
| 新参数 | schema 追加字段 + 默认值 | 无 |
| 新数据契约 | `srsp.<域>.<物>/<主版本>` + schema hash | 无 |
| 破坏性变更 | 能力主版本 +1，旧实现保留至弃用 | 迁移脚本 |

保留档必须显式失败（`UNSUPPORTED`），禁止静默落到默认实现。

## 6. 目录与测试约定

```
src/components/<kind>/<name>/
├─ component.json
├─ CMakeLists.txt          # target: srsp_component_<name>
├─ include/srsp/components/<name>.hpp
├─ src/<name>.cpp
└─ tests/test_<name>.cpp   # label: component.<name>
```

每个组件至少三条测试：参数校验（含缺必填）、生命周期（init/失败路径）、数据契约（schema 往返）；`docs/11` 能力矩阵新增一行并映射到测试，无测试不得留行。

## 7. 修订记录

| 日期 | 版本 | 说明 |
|---|---|---|
| 2026-11-09 | V1 | 从架构规划同步：组件五要素、11 类清单、manifest、生命周期、扩展规则、目录约定 |
