# 工程修改日志

> 记录规则：
> - 按日期分组，只记录日期，不记录具体时间。
> - 同一天的修改按先后顺序编号 V1、V2……；跨天重新从 V1 开始。
> - 每条记录包含：类型、文件、摘要、验证、关联计划。
> - 本文件是该模型命名空间内的唯一修改日志，不拆分。
> - 追加记录优先使用 `tools/ai/Append-ChangeLog.ps1`。

## 2026-10-22

### V1 — 注册 kimi 别名并建立 kimi 模型工作区

- 类型：新增
- 文件：
  - `ai-models.json`
  - `kimi/README.md`
  - `kimi/Temp/.gitkeep`
  - `kimi/Temp/README.md`
  - `kimi/Logs/Log/CHANGELOG.md`
- 摘要：ai-models.json 为 kimi 增加 moonshotai、moonshotai-cn、kimi-k2、kimi-k3 别名，使当前运行模型归一到 kimi 命名空间；用 New-AiModelWorkspace 生成 kimi/{Temp,Logs/Plan,Logs/Log}、README 与 CHANGELOG 头；补 kimi/Temp/.gitkeep 与 Temp/README.md（对齐 deepseek 的入版策略）；kimi/README.md 补别名清单。
- 验证：ai-models.json 解析通过；kimi-k3/moonshotai-cn/kimi/moonshot 均解析为 kimi；Enter-AgentEnv 注入 SRSP_AI_MODEL=kimi、TEMP=kimi/Temp/tmp；8 个必需路径存在；未误建 kimi-k3/ 目录。
- 关联计划：`deepseek/Logs/Plan/2026-10-20-agent-workspace-config.md`

### V2 — 设计文档评审：重点总结与 15 项优化建议

- 类型：文档
- 文件：
  - `kimi/Logs/Plan/2026-10-22-design-review.md`
- 摘要：重读 docs/01-07 与 api/ 现状：总结设计主线（契约先行、能力寻址、双传输、确定性、可离线交付）与 5 条优点；提出 P0 契约正确性 5 项（EntityDelta 缺速度场、state_hash 未定义、time_tai 精度、MCAP arrow 编码冲突、跨平台精度承诺过强）、P1 契约完备性 6 项（订阅机制、checkpoint 统一、错误所有权、lockfile、permissions 适用范围、双网格源）、P2 工程排期 4 项。
- 验证：计划内相对链接全部有效；每个优化点均给出文档/代码定位；未改动 docs 与 api（待评审采纳后再改）。
- 关联计划：`kimi/Logs/Plan/2026-10-22-design-review.md`

### V3 — 优化方案：多语言混合实现与多工具链开发环境

- 类型：文档
- 文件：
  - `kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md`
- 摘要：按用户两条需求制定方案：(1) 平台级多语言政策——官方实现限 C++/Python/Rust 三种，新增宿主侧 core_api.h 与 ABI 增补（禁 bitfield、枚举固定底层类型、三编译器布局断言），manifest 平台键扩展为 os-arch-toolchain；(2) 工具链矩阵——MSYS2 保留为主，Windows 增 MSVC+vcpkg 备选，WSL2/Docker 补充，CI 与预设矩阵扩展，边车优先原则强化。
- 验证：计划内 9 个相对链接全部有效；方案与既有评审 O3/O6/O7/O8 合流；未改动 docs 与 api（待评审关闭 D-7~D-12 后实施）。
- 关联计划：`kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md`

### V4 — 执行多语言/多工具链方案阶段 A：环境闭环、ABI 增补、core_api、MSVC 工具链

- 类型：新增
- 文件：
  - `api/include/srsp/core_api.h`
  - `api/include/srsp/plugin_api.h`
  - `api/schema/plugin-manifest.schema.json`
  - `api/schema/examples/`
  - `tests/abi/`
  - `tests/schema/`
  - `tests/CMakeLists.txt`
  - `CMakeLists.txt`
  - `CMakePresets.json`
  - `vcpkg.json`
  - `examples/hello_plugin/CMakeLists.txt`
  - `examples/hello_plugin/host_demo.cpp`
  - `docs/01-architecture.md`
  - `docs/02-plugin-contract.md`
  - `docs/04-subsystems-and-stack.md`
  - `docs/05-msys2-dev-environment.md`
  - `docs/06-packaging.md`
  - `docs/07-roadmap-risks.md`
  - `README.md`
  - `kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md`
  - `kimi/Logs/Plan/2026-10-22-design-review.md`
- 摘要：D-7~D-12 按建议关闭并执行阶段 A：(1) pacman 安装 CMake 4.4.0/Ninja 1.13.2/ccache/Catch2，win-ucrt64-debug 构建测试通过；(2) plugin_api.h 增补禁位域/枚举 32 位/跨编译器布局条款并追加 time_tai_ns（评审 O3），tests/abi 布局断言建立；(3) manifest schema 支持 os-arch-toolchain 标签（旧键兼容），10 个样例（6 正 4 反）接入 ctest；(4) 新增宿主侧 core_api.h draft 0.1（context/项目/时钟/快照/订阅/透传）；(5) CMakePresets 新增 win-msvc-*（VS 2026 实测）与 win-vcpkg-*，vcpkg.json 建立；(6) 插件静态 CRT 后 GCC 与 MSVC 宿主/插件双向互载成功。文档 01/02/04/05/06/07 与 README 同步更新。
- 验证：win-ucrt64-debug 与 win-msvc-debug 双工具链 configure/build 成功；ctest 3/3 通过（hello_plugin_abi、abi_layout、schema_manifest_examples）；abi_layout 在 GCC 16.1 与 MSVC 19.50 下断言全过；schema 样例 10/10 符合期望；跨工具链混载双向 exit 0；5 个 JSON 文件解析通过。未验证：Linux 构建（无环境）、win-vcpkg-*（VCPKG_ROOT 未配置）、win-msvc-release。
- 关联计划：`kimi/Logs/Plan/2026-10-22-multilang-toolchain-plan.md`
