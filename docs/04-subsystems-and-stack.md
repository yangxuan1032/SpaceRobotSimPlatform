# 04 关键子系统与技术选型

> 本文给出各子系统的职责边界、关键算法/协议选择，以及全平台技术栈与备选方案对比。
> 版本：0.1 ｜ 状态：评审草案

---

## 1. 子系统地图

```
┌─────────────── 平台内核 ───────────────┐
│ 时间 · 坐标系 · 总线 · 调度 · 插件宿主 │
└───┬────────┬────────┬────────┬─────────┘
    │        │        │        │
┌───▼───┐ ┌──▼────┐ ┌─▼─────┐ ┌▼──────────┐
│ 模型   │ │ 动力学 │ │ 环境  │ │ 传感器     │
│ 导入   │ │ 力/接触│ │ 星历  │ │ 相机/雷达  │
└───┬───┘ └──┬────┘ └─┬─────┘ └┬──────────┘
    │        │        │        │
    └────────┴───┬────┴────────┘
                 │ 状态/场景
          ┌──────▼──────┐   ┌─────────────┐
          │   渲染器     │──▶│  UI / 面板   │
          └──────┬──────┘   └─────────────┘
                 │
          ┌──────▼──────┐
          │ 录制 / 回放  │
          └─────────────┘
```

---

## 2. 动力学子系统

### 2.1 能力需求

| 需求 | v1.0 | 说明 |
|---|---|---|
| 浮基多体（树形） | ✅ | 6 自由度基座 + n 个旋转/移动关节 |
| 反作用轮 / CMG | ✅ | 动量交换，轮速作为状态 |
| 推力器 | ✅ | 力 + 力矩 + 质量流 + 最小冲量 |
| 关节摩擦/阻尼/柔性 | ⚠️ | 摩擦与阻尼 ✅；柔性 1.1（先留接口） |
| 接触与捕获 | ⚠️ | 冲量法 + 抓持约束（1.0 基础版） |
| 变步长 / 刚性 | ⚠️ | 插件内部允许；对外固定宏步 |
| 参数在线修改 | ✅ | 质量、增益、摩擦（受求解器支持范围限制） |
| 确定性 | ✅ | 同输入同输出 |

### 2.2 状态表示与算法

- 广义坐标：基座四元数（4）+ 位置（3）+ 关节（n）→ `nq = 7 + n`；广义速度 `nv = 6 + n`。
- 质量矩阵：CRBA（O(n²)，n ≤ 200 足够）或 ABA（O(n)）；两者都实现，按模型规模自动选择。
- 动力学方程：`M(q)·ν̇ + h(q,ν) = Sᵀτ + Jᵀf_ext + τ_env`。
- 积分器：
  - `semi_implicit_euler`：默认，稳定、快，适合可视化；
  - `rk4`：精度基准；
  - `generalized_alpha`：刚性/柔性系统（1.1）；
  - 四元数用 Lie 群更新（`q ← q ⊗ exp(ω_b·dt/2)`，右乘对应体坐标系角速度），每步归一化。
- 约束：v1 只支持树形；闭链/抓持用外部约束求解（接触插件）。
- 力模型聚合：动力学插件接收"广义力总成"，不重复计算环境力。

### 2.3 求解器插件矩阵

| 插件 | 实现 | 定位 | 风险 |
|---|---|---|---|
| `dynamics.rigid` | 自研 C++/Eigen | **默认**，完全可控、可确定性验证 | 开发量大，需数值验证 |
| `dynamics.pinocchio` | Pinocchio 3.x（可选链接） | 交叉验证、复杂关节 | MSYS2 无包，需 superbuild |
| `dynamics.mujoco` | MuJoCo 3.x 边车 | 富接触、捕获、摩擦 | 进程间开销；许可证 Apache-2.0 |
| `dynamics.pybullet` | PyBullet 边车 | 快速原型、教学 | 精度与确定性较弱 |

自研求解器必须通过与 Pinocchio 的**黄金模型对比测试**（同一 URDF/SRM，随机状态 1000 组，比较 `M`、`h`、`J`，相对误差 < 1e-10）。

### 2.4 验证清单

- 无外力时线动量/角动量守恒（1e-12/步累积阈值）；
- 双体开普勒轨道解析解对比（1 圈，位置误差 < 1 m）；
- 单摆/双摆能量漂移（RK4，10⁵ 步 < 1e-6）；
- 轮控卫星的角动量交换守恒；
- 与 Pinocchio 的 `M/h/J` 对比；
- 确定性：同机重复运行 `state_hash` 完全一致。

---

## 3. 力模型、接触与捕获

### 3.1 力模型

| 力模型 | 输入 | 说明 |
|---|---|---|
| 中心引力 / 球谐引力 | 位置、时间、重力场系数 | EGM2008，阶数可配（实时 20–50 阶，离线 360 阶） |
| 第三体引力 | 星历 | 日月点质量，含光行时修正（可选） |
| 大气阻力 | 位置、速度、姿态、空间天气 | NRLMSISE-00 + 旋转大气 |
| 太阳光压 | 太阳方向、遮挡因子、面积/反射率 | 球模型 + 面元模型（1.1） |
| 重力梯度 | 惯量、姿态 | 可由球谐引力自然包含；独立模型用于简化 |
| 磁力矩 | 磁矩、磁场 | IGRF-14 + 磁力矩器 |
| 推力器 | 指令 | 力、力矩、质量流、上升/下降时间 |
| 柔性耦合 | 模态坐标、模态力 | 1.1 |
| 气动/羽流干扰 | 姿态、高度 | 1.2（预留） |

所有力模型实现 `srsp.dynamics/force@1`，按注册顺序累加；是否启用由模型 `force_models` 与场景环境共同决定。

### 3.2 接触与捕获

- 检测：凸体 GJK/EPA（FCL/Coal 或自研），支持盒、球、胶囊、凸包、三角网格。
- 求解：顺序冲量 + 库仑摩擦锥（近似金字塔），固定迭代次数（默认 20）保证确定性。
- 捕获：抓持接口用"软约束 → 刚性焊接"两阶段；对齐误差、冲击力、碰撞冲量写入 `sim.event`。
- 备选：MuJoCo 边车用于高保真接触（软接触、椭圆摩擦锥）。

---

## 4. 空间环境子系统

### 4.1 组成

| 能力 | 推荐实现 | 数据 | 精度/性能 |
|---|---|---|---|
| 星历 | NAIF CSPICE | SPK（DE440）、PCK、LSK、FK | 米级；查询 µs 级 |
| 时间/岁差章动 | ERFA（SOFA） | 内置模型 | IAU 2006/2000A |
| 轨道传播 | SGP4（TLE）+ 数值积分 | TLE、初始根数 | SGP4 km 级；数值积分高精度 |
| 重力场 | 自研球谐 | EGM2008 / GGM05S | 20–360 阶可配 |
| 大气 | NRLMSISE-00 | F10.7/Ap 表 | 经验模型 |
| 磁场 | IGRF-14 | 系数文件 | 阶数 13 |
| 星空 | Hipparcos/Tycho-2 子集 | 二进制星表 | 10⁵ 颗，点精灵 |
| 光照 | 自研（几何阴影 + 辐照度） | 太阳常数、反照率 | 解析 |

> **环境设计也是插件**：场景搭建/环境编辑由 UI 侧插件完成，产物是 `srsp.scene/1` 场景包 + 环境配置；内核只负责按能力解析与执行。第三方可用自己的编辑器生成同一格式，不依赖官方界面。

### 4.2 光照与阴影

- 太阳方向来自星历；辐照度 `1361 W/m²`（可配）。
- Eclipse：有限半径圆锥阴影模型，输出 `umbra/penumbra` 因子（0–1 连续），供电源、热、传感器、渲染共用。
- 地球反照：简化半球积分（v1 常数反照率），面元级在 1.1。
- 渲染侧与物理侧共用同一 `env.lighting` 输出，避免"看起来有光但电源没电"的不一致。

### 4.3 数据管理

- 星历/重力/大气数据以"数据包"形式随插件分发，带 SHA-256 与版本；
- 数据目录可配置（`SRSP_DATA_DIR`），支持只读共享盘；
- 缺失数据时插件必须报 `SRSP_E_NOT_FOUND` 并给出下载/放置说明，不得静默降级。

---

## 5. 传感器子系统

| 传感器 | 模型要点 | 依赖 |
|---|---|---|
| IMU | 真实加速度/角速度 + 零偏不稳定性 + 角度随机游走 + 量化 | 动力学 |
| 星敏感器 | 星表投影 + 质心提取 + 噪声 → 四元数；遮挡/杂光 | 星历 + 星表 + 渲染（可选） |
| 太阳敏感器 | 太阳矢量在本体系 + 地球遮挡 | 星历 + 光照 |
| 相机 | 针孔 + 畸变 + 曝光/增益/噪声/运动模糊 | 渲染器离屏 |
| LiDAR | 扫描模式 + 射线求交 + 强度 + 噪声 | 渲染深度或 CPU BVH |
| GNSS | 轨道位置 + 电离层/对流层简化 + 可见星 | 星历 |
| 力/力矩 | 关节/基座反力 + 噪声 | 动力学 |
| 关节编码器 | 量化 + 齿槽误差 | 动力学 |

**确定性**：传感器噪声由 `(seed, sensor_id, seq)` 派生；相机渲染除外（GPU 非确定），需在文档与测试中显式标注。

---

## 6. 渲染子系统

### 6.1 需求

| 编号 | 需求 | 优先级 |
|---|---|---|
| R1 | 轨道尺度场景（10³–10⁹ m）无抖动 | P0 |
| R2 | 地球（纹理、云、夜灯、大气、晨昏线） | P0 |
| R3 | 星空、太阳、月球，正确光照方向 | P0 |
| R4 | PBR 材质、软阴影、后处理（ACES、Bloom） | P0 |
| R5 | 轨迹、坐标系、标签、传感器视锥等覆盖层 | P0 |
| R6 | 多视口 + 离屏相机成像（RGB/深度/分割） | P0 |
| R7 | 拾取、选择、高亮 | P1 |
| R8 | 大点云（LiDAR/重建） | P1 |
| R9 | 无头渲染与截图回归 | P1 |
| R10 | Web/远程查看 | P2 |

### 6.2 候选对比

| 方案 | 优势 | 劣势 | 结论 |
|---|---|---|---|
| **bgfx** | 统一 RHI（D3D11/12、Vulkan、GL/GLES、Metal）、BSD-2、稳定 C API、可控管线 | 无场景图/资源管理，需自建；无原生 Wayland | **主渲染器** |
| Qt Quick 3D | 与 Qt 同栈、零额外依赖、跨平台（含 Wayland）、PBR/阴影/实例化开箱即用 | 自定义着色器与多视口受限，离屏成像别扭 | **轻量预览/兜底** |
| VTK | 科学可视化强、体渲染/点云成熟 | 重、启动慢、地球/大气效果差 | 分析视图插件（后续） |
| OpenSceneGraph | 航天/GIS 生态、osgEarth | 技术栈老、依赖多、构建复杂 | 不选 |
| three.js / WebGPU | 远程/无安装、生态好 | 大场景性能与精度有限 | 远程查看插件 |
| Unity / Unreal | 画质上限高 | 许可、体积、进程间集成成本 | 边车选项（非默认） |
| Filament | 移动端 PBR 优秀 | MinGW 支持弱、构建重 | 不选 |

### 6.3 默认渲染器设计（bgfx）

- **窗口**：Windows 传入 `HWND`（Qt `winId()`）；Linux 传入 X11 `Window`（Qt xcb）。Wayland 原生不支持，v1 要求 X11/XWayland 会话；Wayland 用户自动降级到 Qt Quick 3D 渲染器或提示切换会话。
- **双原点渲染**：
  - 近端 pass：原点 = 机器人/摄像机附近的浮动原点，float32，近裁面 1 cm、远裁面 100 km（可配）；
  - 远端 pass：原点 = 地球中心（或太阳），单独 framebuffer，反转 Z；
  - 合成：按线性深度重建后合成，保证地球遮挡机器人时的正确关系。
- **地球**：v1 用 8K Blue Marble + 法线/夜灯贴图 + 大气壳（单次散射近似）+ 云层（可选）；v2 用四叉树瓦片 LOD。
- **星空**：星表点精灵，按星等映射大小/亮度，随岁差旋转；远景单独 pass，不写深度。
- **阴影**：机器人局部级联阴影（2–3 级），地球阴影用解析 eclipse，不用 shadow map。
- **覆盖层**：内置立即模式绘制（线/点/文本），文本用字体图集（stb_truetype），避免渲染器依赖 Qt。
- **传感器成像**：独立 framebuffer + 异步回读（PBO/暂存缓冲），支持 RGB8/RGBA8/深度/ID；多相机按预算时间片轮转。
- **拾取**：ID buffer 或 CPU 射线（BVH），优先 ID buffer（一次渲染，支持实例）。
- **帧节奏**：渲染线程持有最新快照；若新快照未到，用上一帧 + 外推；`generation` 判断丢弃过期增量。

### 6.4 性能预算（1080p / 60 fps）

| 阶段 | 预算 |
|---|---|
| 可见性/剔除 | 1.5 ms |
| 阴影 | 2.0 ms |
| 近端 pass | 4.0 ms |
| 远端 pass（地球/星空） | 3.0 ms |
| 覆盖层 + 文本 | 1.5 ms |
| UI 合成 | 1.0 ms |
| 余量 | 3.6 ms |

传感器成像（3×640×480 @30 Hz）单独计量，目标是主视口帧率下降 < 10%。

### 6.5 渲染器与 UI 的边界

- 渲染器不创建 Qt 控件，只接收原生窗口句柄；
- UI 通过 `srsp.render/renderer` 能力调用，不直接调用 bgfx；
- 覆盖层内容由内核生成（业务知道"这是什么"），渲染器只负责画；
- 截图/录像由渲染器 `capture` 提供，UI 负责保存与展示。

---

## 7. 界面子系统

### 7.1 技术选择

- **Qt 6 Widgets 为主 + QML 局部嵌入**：桌面工具需要成熟的停靠、表格、树、属性编辑；QML 用于仪表盘、时间轴、现代风格面板。
- **停靠**：v1 用 `QMainWindow + QDockWidget`（零额外依赖、LGPL）；需要更强能力时再评估 Qt Advanced Docking System（LGPL）或 KDDockWidgets（注意 GPL/商业双许可）。
- **图表**：优先 **Qwt（LGPL）** 或 **ImPlot（MIT）**；避免 Qt Charts（GPL/商业）带来许可约束。
- **3D 视口**：`QWidget` 设置 `WA_NativeWindow`，把 `winId()` 交给渲染插件；UI 不参与 OpenGL/Vulkan 上下文管理。
- **面板插件边界**：Qt 面板插件必须与平台使用同一 Qt 版本与编译器（Qt 插件 ABI 不跨版本兼容）；第三方或跨语言 UI 建议做成 Web 面板（`QWebEngineView` + gRPC/WebSocket），避免 Qt ABI 约束。

### 7.2 主要面板

| 面板 | 职责 |
|---|---|
| 场景树 | 实体层级、可见性、选择联动 |
| 属性编辑器 | 由 JSON Schema 驱动生成控件，支持单位换算与校验 |
| 插件管理器 | 安装/启用/禁用/版本/权限/健康状态 |
| 时间轴 | 播放、暂停、单步、倍速、跳转、事件标记 |
| 曲线视图 | 任意数值话题、实时/回放、导出 CSV |
| 传感器视图 | 相机画面、点云、IMU 曲线、星敏视场 |
| 控制台 | 日志、诊断、脚本 REPL（转发到 Python 边车） |
| 任务/脚本 | 批处理任务、脚本编辑与运行 |

### 7.3 交互与可达性

- 中英文双语（Qt Linguist，`zh_CN`/`en_US`），默认跟随系统；
- 高 DPI、深浅色主题、键盘快捷键可配置；
- 大文件/长名称的省略与 tooltip；
- 所有耗时操作可取消，UI 不阻塞（异步任务 + 进度）。

---

## 8. 脚本与自动化

### 8.1 Python 边车

- 独立进程 `srsp-bridge`，通过 gRPC + 共享内存接入；
- 提供 `srsp` Python 包：`Session`、`Topic`、`State`、`Commands`、`Recorder`；
- 依赖：`grpcio`、`protobuf`、`pyarrow`、`numpy`；可选 `scipy`、`matplotlib`、`casadi`；
- 不作为内嵌解释器：规避 MinGW 与 python.org（MSVC）ABI 不兼容、GIL 与崩溃传染。

### 8.2 使用方式

```python
from srsp import Session

with Session.connect("127.0.0.1:53117", token="...") as s:
    s.load_project("capture_demo.srproj")
    s.run(steps=1000)
    st = s.state()                    # Arrow Table
    print(st["bodies"]["pos_w"].to_pylist()[-1])
```

- 批处理：`srsp-cli run --scenario x.json --script approach.py --record out.mcap`
- 参数扫描：`srsp-cli sweep --param panel_length=3.5:5.0:0.5`，内部并行多进程。

### 8.3 其他语言

MATLAB/Simulink 通过边车（MATLAB Engine 或编译为 C 共享库）；Julia 通过 gRPC；C# 通过 grpc-dotnet（可承载 Unity 渲染器）。

---

## 9. 记录、回放与分析

| 环节 | 方案 |
|---|---|
| 容器 | MCAP（zstd、索引、内嵌 schema） |
| 写入 | 独立 IO 线程，无锁队列，批量写 chunk |
| 元数据 | `run.json`：构建、git、插件版本、种子、模型哈希、硬件 |
| 回放 | `ReplayClock` 驱动；支持变速/跳转/逐帧 |
| 分析 | Parquet/CSV 导出；DuckDB/pandas；Foxglove Studio 直开 |
| 回归 | `state_hash` 逐宏步比对；图像 SSIM |

---

## 10. 技术栈总表

| 领域 | 选型 | 版本基线 | Windows/MSYS2 | 备选 |
|---|---|---|---|---|
| 内核语言 | C++20 | GCC 13+ / Clang 17+ | ✅ GCC 16.1（本机 UCRT64） | Rust（部分插件） |
| 备选构建工具链 | MSVC 2022+（vcpkg） | 19.50+ | ✅ VS 2026 实测（`win-msvc-*` 预设） | Clang-CL |
| 构建 | CMake + Ninja + Presets | CMake ≥ 3.27 | ✅（需安装） | Meson（不选） |
| 包管理 | MSYS2 pacman / apt / superbuild | — | ✅ | Conan 2（备选） |
| 数学 | Eigen 3.4 + Sophus | 3.4 / 1.22 | 需核实 | Blaze（不选） |
| GUI | Qt 6 Widgets + QML | 6.6+（MSYS2 仓库 6.11.2） | ✅ 已确认 | — |
| 3D | bgfx（窗口句柄由 Qt 提供） | 1.1x | 需核实/自建 | Qt Quick 3D |
| 物理（可选） | Pinocchio / MuJoCo / Bullet | 3.x / 3.x / 3.2 | ❌ / ❌ / 需核实 | ODE |
| RPC | gRPC + Protobuf | 1.60+ / 3.21+ | ✅ 已确认 1.82 | ZeroMQ（不选） |
| 数组 | Apache Arrow C Data Interface | 15+ | 需核实 | FlatBuffers（备选） |
| JSON | nlohmann/json + JSON Schema 2020-12 | 3.11+ | 需核实 | RapidJSON |
| TOML | toml++ | 3.x | 需核实 | — |
| 日志 | spdlog + fmt | 1.14+ / 10+ | 需核实 | — |
| CLI | CLI11 | 2.4+ | 需核实 | — |
| 测试 | Catch2 v3 + Google Benchmark | 3.6+ / 1.8+ | 需核实 | GoogleTest |
| 录制 | MCAP + zstd | 0.10+ | 需核实/自建 | 自研容器（不选） |
| 星历 | NAIF CSPICE | N0067+ | ❌ 自建 | Orekit（Java 边车） |
| 时间/岁差 | ERFA (SOFA) | 2024 | 需核实 | CSPICE 内置 |
| 轨道 | SGP4 + 自研数值积分 | — | 自建 | Orekit |
| 重力场 | 自研球谐 + EGM2008 | — | 数据 + 自研 | — |
| 大气 | NRLMSISE-00 | — | 自建（C 代码） | Jacchia-Bowman |
| 磁场 | IGRF-14 | — | 数据 + 自研 | — |
| 性能剖析 | Tracy | 0.11+ | 需核实/自建 | Superluminal |
| 文档 | MkDocs Material + Doxygen | — | ✅ | Sphinx |
| CI | GitHub Actions + msys2/setup-msys2 | — | ✅ | GitLab CI |

> "需核实/自建"的含义：MSYS2 官方仓库是否存在对应 UCRT64 包尚未逐一确认；`tools/env/check-msys2-packages.sh` 可在目标机器上一键核对并输出缺失清单。"已确认"表示已在 MSYS2 包页面或本机安装中核实。

---

## 11. 跨语言接入矩阵

| 语言 | 接入方式 | 进程内 | 边车 | 适用组件 |
|---|---|---|---|---|
| C / C++ | C ABI 直接导出 | ✅ | ✅ | 内核、动力学、渲染 |
| Rust | `cdylib` + `extern "C"`（prost/serde） | ✅ | ✅ | 高性能插件 |
| Python | `srsp-bridge` + pyarrow | ❌（不内嵌） | ✅ | 控制器、脚本、AI |
| C# / .NET | grpc-dotnet | ❌ | ✅ | Unity 渲染、工具 |
| Java / Kotlin | gRPC-Java | ❌ | ✅ | Orekit、企业集成 |
| MATLAB | MATLAB Engine / 编译为 C 库 | ⚠️（编译后） | ✅ | 控制设计、GNC |
| Julia | gRPC + Arrow | ❌ | ✅ | 优化、科学计算 |
| JavaScript/TS | WebSocket / gRPC-Web | ❌ | ✅ | Web 查看器 |

**语言选择建议**：性能敏感的动力学/渲染用 C++/Rust；算法验证用 Python；企业/遗产代码用 Java/MATLAB 边车；不要为了"统一语言"牺牲生态。

---

## 12. 明确不采用及理由

| 技术 | 不采用理由 |
|---|---|
| 内嵌 CPython | MinGW 与官方 Python 的 MSVC ABI 不兼容；GIL 与崩溃传染；打包体积与版本冲突 |
| 以 MSVC 为主工具链 | 与 MSYS2 约束冲突；混用 MinGW/MSVC 二进制会产生 C++ ABI 不兼容 |
| vcpkg 默认 trio | MSVC 预编译库为主，MinGW triplet 支持有限，易与 pacman 冲突 |
| Electron 作为主 UI | 体积、GPU 集成、原生窗口能力弱 |
| Unity/Unreal 作为平台基座 | 许可证、运行时体积、工程集成成本；仅作为可选渲染边车 |
| ROS 2 作为核心依赖 | 引入 DDS/构建体系与部署复杂度；可作为可选桥接插件 |
| Qt Charts / QCustomPlot | GPL/商业许可约束；改用 Qwt(LGPL)/ImPlot(MIT) |
| 自研脚本语言 | 生态为零；脚本能力交给 Python 边车 |

---

## 13. 版本基线（构建矩阵）

| 平台 | 编译器 | 构建 | 运行 |
|---|---|---|---|
| Windows 10 1809+ / 11 x64 | GCC 13–16（UCRT64）/ Clang 17+（CLANG64） | CMake 3.27+ + Ninja | UCRT 运行时，Win64 |
| Ubuntu 22.04 x64（glibc 2.35） | GCC 12+ / Clang 15+ | CMake 3.27+ + Ninja | X11/XWayland，GL 4.1 / Vulkan 1.1 |
| Ubuntu 24.04 x64 | GCC 13+ | 同上 | 同上 |

- C++ 标准：`-std=c++20`（禁止依赖编译器扩展）；C++23 特性需特性测试宏保护。
- 警告：`-Wall -Wextra -Wpedantic -Wconversion`（逐步开启），CI 中 `-Werror` 仅对新增代码。
- 浮点：禁止 `-ffast-math`；`-ffp-contract=off`（保证确定性）。

---

## 14. 与同类仿真工具的定位对比（2026-10）

> 完整调研见 `deepseek/Logs/Plan/2026-10-23-simulator-landscape.md`。这里只保留对技术选型有约束力的结论。

### 14.1 四类工具与代表实现

| 类别 | 代表 | 对 SRSP 的意义 |
|---|---|---|
| 通用机器人仿真 | MuJoCo/MJX、Isaac Sim + Isaac Lab、Gazebo、PyBullet、Drake | 接触、GPU 并行与 RL 生态的参照；**不与它们拼训练吞吐** |
| 空间机器人 / OOS | mjorbit（MuJoCo + 航天器扩展）、Space ROS、OnOrbitROS、Astrobee 仿真 | 最接近的竞品；mjorbit 证明「复用成熟引擎 + 空间扩展」可行 |
| 轨道与 GNC | STK、GMAT、Orekit、Basilisk、Tudat、Nyx、FreeFlyer | 空间环境的精度基准与对照实现 |
| 可视化 / 地球渲染 | Cesium（CesiumJS、for Unreal/Unity）、STK 可视化 | 轨道尺度渲染的参照；我们以双原点 + 反转 Z 自研 |

### 14.2 对选型的约束

1. **接触与柔顺不重复造轮子**：v1 自研 `dynamics.rigid` 覆盖刚性与基础接触；高保真接触走 `dynamics.mujoco` 边车（MuJoCo 3.x），并在 M1 内做原型验证。
2. **精度对标而非自证**：环境与轨道模块的验收必须给出与 **Orekit / GMAT（或 Basilisk）** 的同工况偏差表；参考 Basilisk 公开对比的做法（同初值、同模型参数、公开米级偏差），并把脚本入库为可复现基准。
3. **容差分级**：跨工具/跨平台只承诺米级或相对容差（Basilisk 实测：无阻力 30 天约 0.02–1.2 m，Molniya 全摄动 7–39 m，400 km 带阻力 17–72 m）；同平台同构建才承诺位级一致（`state_hash`）。
4. **互操作优先于替代**：对外提供 MJCF / MCAP / glTF / SRM 的导入导出，让用户的既有资产（MuJoCo 模型、录制数据）能进来，而不是要求迁移。

### 14.3 差异化（写入产品定位）

| 差异化 | 说明 |
|---|---|
| 工程级确定性 | 逐宏步 `state_hash`、可复现录制、同平台位级回放 |
| 空间尺度可视化 | 双原点 + 反转 Z + eclipse + 星表，原生支持 10⁷ m 场景 |
| 能力寻址插件 | 场景依赖能力而非插件 ID，院所既有算法（Python/MATLAB/C++）可直接接入 |

对应的非目标已写入 [01 §1.3](01-architecture.md)（不做 GPU 并行 RL 训练平台），风险见 [07 R15](07-roadmap-risks.md)。

调研中竞品的其余优势能力（GPU 批量并行、可微分物理、高保真接触、合成数据管线、任务级分析、FSW/HIL、地球 3D Tiles 等）已整理为**预留扩展点**，不进入 v1 实施：[07 §11](07-roadmap-risks.md) 给出能力映射与触发条件，[02 §13](02-plugin-contract.md) 给出预留能力 ID 与调用约定。
