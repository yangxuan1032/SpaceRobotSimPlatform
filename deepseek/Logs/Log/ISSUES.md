# 开发问题与解决方案记录（ISSUES）

> 记录开发过程中**非预期问题**的现象、根因与解决方案，与变更日志（`CHANGELOG.md`）互补：
> CHANGELOG 记「改了什么」，本文件记「踩了什么坑、怎么出来的」。
> 规则来源：`tools/ai/skills/work-log/SKILL.md` §2.2。

- 编号：同一天按先后顺序 `P1、P2……`，跨天重新从 `P1` 开始
- 只写日期，不写时间
- 每条必含：现象 / 根因 / 解决 / 验证；能关联 CHANGELOG 条目的要写出
- 只记录**有复用价值**的问题；一次性的笔误、闲聊、无结论的猜测不记
- 历史条目保留当时的路径与命令，不追改

---

## 2026-10-22

### P1 — 沙箱内 MSYS2 子进程无法启动

- 现象：`sh.exe`、`pacman`、`g++` 报 `couldn't create signal pipe, Win32 error 5`；构建无输出但退出码 1。
- 根因：DSH 沙箱禁止命名管道，MSYS2 程序启动依赖管道；受限模式下编译器中间文件写入也受限。
- 解决：安装、构建、测试统一改为一次性完全权限执行（`danger-full-access`，逐次申请）；只读检查仍走受限模式。
- 验证：四套预设构建与测试全部通过。
- 关联：`CHANGELOG.md` 2026-10-22 V1–V3。

### P2 — MSYS2 包名错误（catch2 → catch）

- 现象：安装 `mingw-w64-ucrt-x86_64-catch2` 失败，仓库中无此包。
- 根因：MSYS2 的包名是 `catch`，`catch2` 是项目名。
- 解决：修正 `tools/env/packages.txt` 与 `docs/05` 的包名。
- 验证：pacman 安装成功，CMake 找到 Catch2 3.15.2。
- 关联：`docs/05-msys2-dev-environment.md`。

### P3 — MSVC 预设被 MSYS2 的 cc/c++ 抢占

- 现象：`win-msvc-debug` 预设实际调用 GCC，MSVC 未参与构建。
- 根因：PATH 中 `cc`/`c++` 先于 `cl`，CMake 探测到 MinGW 工具链。
- 解决：导入 `vcvars64.bat` 环境，并显式设置 `CC=cl`、`CXX=cl`。
- 验证：`CMAKE_CXX_COMPILER` 指向 `cl.exe`，构建通过。

### P4 — 跨工具链加载插件失败（0xC0000135）

- 现象：GCC 宿主加载 Clang 构建的插件时报缺少 DLL。
- 根因：插件动态依赖 `libstdc++`/`libgcc`，目标目录没有对应运行时。
- 解决：插件一律静态链接 CRT（GCC/Clang `-static-libgcc -static-libstdc++`，MSVC `/MT`），并写入 ABI 契约。
- 验证：GCC↔Clang 双向加载成功；CLANG64（libc++）插件可加载。
- 关联：`docs/02-plugin-contract.md` ABI 条款。

### P5 — schema 测试抓到系统 Windows Python

- 现象：CMake 找到 Windows Store 的 Python，缺少 `jsonschema`。
- 根因：未指定解释器，PATH 顺序决定结果。
- 解决：新增隐藏预设 `win-python` 固定 `Python3_EXECUTABLE` 为 UCRT64 Python；安装 `python-jsonschema`。
- 验证：`schema_manifest_examples` 通过。

---

## 2026-10-23

### P1 — Enter-AgentEnv.ps1 污染调用方变量

- 现象：点号引用脚本后，调用方的 `$root` 被清空，后续路径拼接全部失效。
- 根因：脚本内部使用通用变量名（`$Root`），PowerShell 变量名大小写不敏感。
- 解决：参数改为 `-ProjectRoot`（保留旧名作别名），内部变量统一 `$srsp*` 前缀。
- 验证：隔离目录测试，调用方变量保持不变。
- 关联：`CHANGELOG.md` 2026-10-22 条目。

### P2 — git push 凭据助手失败

- 现象：`couldn't create signal pipe, Win32 error 5`，推送在读取凭据阶段失败。
- 根因：Git Credential Manager 经 `sh.exe` 启动，沙箱禁止命名管道。
- 解决：临时 `GIT_ASKPASS` 注入凭据 + `-c credential.helper=` 禁用助手，用后立即清理。
- 验证：推送成功，远端 HEAD 与本地一致。
- 关联：`work-log` §4.4。

### P3 — 本地代理导致 TLS 中断

- 现象：`TLS connect error: error:0A000126:SSL routines::unexpected eof while reading`，`ls-remote` 同样失败。
- 根因：git 配置 `http.proxy=http://127.0.0.1:7897`，端口在监听但 TLS 握手被中断；直连正常。
- 解决：`git -c http.proxy= push` 绕过代理推送。
- 验证：`ls-remote` 与 `push` 均成功。
- 关联：`work-log` §4.4。

### P4 — commit message 被 shell 拆分

- 现象：`git commit -m @'...'@` 报 `error: pathspec 'RL' did not match`。
- 根因：多行 here-string 含引号时作为参数传递被拆分。
- 解决：提交信息写入 UTF-8（无 BOM）文件，使用 `git commit -F`。
- 验证：提交成功且正文完整。

### P5 — UTF-8 BOM 进入提交标题

- 现象：`git log --oneline` 显示 `﻿docs: ...`。
- 根因：PowerShell 5.1 的 `Set-Content -Encoding utf8` 会写 BOM。
- 解决：改用 `[IO.File]::WriteAllText(..., UTF8Encoding($false))`；已提交的用 `--amend` 修正。
- 验证：提交标题无 BOM。

### P6 — 源码扫描口径错误

- 现象：扫描报告 36 个"源文件"含中文，与事实不符。
- 根因：`Get-ChildItem -Include` 未配合通配路径，实际扫描了全部文件（含 md/json）。
- 解决：先按扩展名过滤（`.h/.hpp/.cpp`）再扫描。
- 验证：6 个 C/C++ 文件全部纯英文；配置类文件同样为英文。
- 关联：`CHANGELOG.md` 2026-10-23 语言规则条目。

### P7 — Catch2 与工具链 ABI 不匹配

- 现象：CLANG64 与 MSVC 下 4 个测试显示 `***Not Run`，MSVC 整棵树构建失败。
- 根因：`find_package(Catch2 3 CONFIG)` 找到 UCRT64 的 Catch2（libstdc++ 构建），拿去链接 libc++/MSVC 目标必然失败，并连带阻断其他目标。
- 解决：`<model>/ModuleTest/CMakeLists.txt` 增加工具链门槛——仅 UCRT64（GCC/Clang）启用模块测试，其余工具链跳过并打印原因。
- 验证：UCRT64 GCC/Clang 9/9，CLANG64 4/4，MSVC 4/4。
- 关联：`CHANGELOG.md` 2026-10-23 内核骨架条目。

### P8 — 注册了未构建的测试

- 现象：`Unable to find executable: .../test_plugin_host.exe`。
- 根因：测试目标因缺少依赖被跳过，但 `add_test` 仍然注册。
- 解决：仅当目标存在时才 `add_test`，否则只输出状态信息。
- 验证：`ctest` 不再出现 "Not Run"。

### P9 — 计划状态判定误匹配

- 现象：`- 状态：实施中（环境与契约部分已完成…）` 被判定为"已完成"，差点误删 M0 计划。
- 根因：正则 `已完成|已废弃` 未锚定，括号内的补充说明也被匹配。
- 解决：改为 `^- 状态：\s*(已完成|已废弃)`，只看状态关键字；分析/调研/摘要类文档按文件名豁免。
- 验证：13 份计划重扫，4 份命中、9 份保留，与策略一致。
- 关联：`work-log` §3。

---

## 2026-10-24

### P1 — printf 输出二进制缓冲区触发类型冲突

- 现象：`main.cpp` 编译报 `conditional expression between distinct pointer types 'uint8_t*' and 'const char*' lacks a cast`。
- 根因：`srsp_buffer::data` 是 `uint8_t*`，与字符串字面量混用在条件表达式里；GCC 拒绝隐式转换（MSVC 更宽松，容易漏掉）。
- 解决：先用 `reinterpret_cast<const char*>` 转换，再参与条件表达式与 `%.*s` 输出。
- 验证：四端构建通过，`integration.cli_demo_inproc` 的 PASS_REGULAR_EXPRESSION 命中。
- 关联：`CHANGELOG.md` 2026-10-24 条目。

### P2 — PowerShell 双引号字符串里的 JSON 片段被拆成参数

- 现象：调用 `Append-ChangeLog.ps1` 时报 `Cannot find drive. A drive with the name 'ok\' does not exist`。
- 根因：PowerShell 双引号字符串用反引号转义，`\"` 不生效，`{"ok":true}` 提前终止字符串并产生额外参数。
- 解决：参数里的 JSON 片段改用单引号字符串，或不写引号（只描述“正则匹配通过”）。
- 验证：日志脚本正常追加。
- 关联：`CHANGELOG.md` 2026-10-24 条目。
