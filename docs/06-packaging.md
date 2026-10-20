# 06 打包、分发与跨平台交付

> 目标：一次构建产出可离线安装、可重定位、可增量升级的 Windows/Linux 交付物，并让插件成为独立可分发的单元。
> 版本：0.1 ｜ 状态：评审草案

---

## 1. 交付物矩阵

| 产物 | 平台 | 形式 | 用途 |
|---|---|---|---|
| `SRSP-<ver>-win64.zip` | Windows | 便携目录 | 免安装、内网拷贝、CI |
| `SRSP-<ver>-win64.msi` / `.exe` | Windows | 安装器 | 正式交付、开始菜单、文件关联 |
| `SRSP-<ver>-x86_64.AppImage` | Linux | 单文件 | 跨发行版便携运行 |
| `srsp_<ver>_amd64.deb` | Debian/Ubuntu | 包 | 系统集成、依赖声明 |
| `srsp-<ver>-linux-x86_64.tar.gz` | Linux | 便携目录 | 服务器/无头部署 |
| `*.srpkg` | 全平台 | 插件包 | 插件独立发布与升级 |
| `srsp-sdk-python-<ver>.whl` | 全平台 | Python wheel | 脚本与自动化 |
| `sbom.json` / `SHA256SUMS` | 全平台 | 元数据 | 合规与校验 |
| 符号包 `*.pdb` / `*.debug` | 全平台 | 独立归档 | 崩溃分析 |

命名规范：`<产品>-<语义版本>-<平台标签>`，平台标签固定为 `win64`、`linux-x86_64`、`linux-aarch64`（预留）。

---

## 2. 运行时目录布局（可重定位）

```
SRSP-1.0.0-win64/
├─ bin/
│  ├─ srsp-studio.exe
│  ├─ srsp-cli.exe
│  ├─ srsp-bridge.exe            # Python 边车启动器（或 .py + 解释器）
│  ├─ srsp_core.dll              # 内核（与 exe 同目录，保证 DLL 搜索命中）
│  ├─ libstdc++-6.dll            # MinGW 运行库（随包）
│  ├─ libgcc_s_seh-1.dll
│  ├─ libwinpthread-1.dll
│  ├─ Qt6Core.dll  Qt6Gui.dll  Qt6Widgets.dll ...
│  └─ platforms/qwindows.dll     # Qt 平台插件（windeployqt 生成）
├─ plugins/                      # SRSP 插件（非 Qt 插件）
│  └─ org.srsp.dynamics.rigid/
│     ├─ plugin.json
│     └─ bin/srsp_dynamics_rigid.dll
├─ share/
│  ├─ srsp/
│  │  ├─ schemas/                # 运行时校验用
│  │  ├─ fonts/
│  │  ├─ stars/                  # 星表
│  │  └─ earth/                  # 基础地球纹理（可选高清包）
│  ├─ doc/
│  └─ licenses/
├─ VERSION.json                  # 构建信息、git hash、依赖清单
└─ LICENSES/                     # 第三方许可证全文
```

要点：

- **可重定位**：所有路径相对可执行文件解析，不写死安装目录；`VERSION.json` 记录构建信息。
- **Qt 插件不与 SRSP 插件混放**：Qt 平台插件固定在 `bin/platforms`，SRSP 插件在 `plugins/`。
- **插件依赖**：插件自带 DLL 与其同目录；加载器使用 `AddDllDirectory` 把插件目录加入搜索路径，避免全局污染。
- **数据与程序分离**：高精度星历、重力场、地球瓦片可作为独立"数据包"安装到 `share/srsp/data/` 或 `SRSP_DATA_DIR`。

Linux 布局遵循 FHS：

```
/usr/bin/srsp-studio
/usr/lib/srsp/libsrsp_core.so.1
/usr/lib/srsp/plugins/...
/usr/share/srsp/{schemas,stars,earth}
/usr/share/doc/srsp/
```

---

## 3. Windows 打包

### 3.1 依赖收集

对每个 `.exe/.dll` 递归解析导入表：

```bash
# MSYS2 中
ldd srsp-studio.exe | awk '{print $3}' | grep -i '/ucrt64/bin' | sort -u
# 或
objdump -p srsp-studio.exe | grep "DLL Name"
```

脚本 `tools/packaging/collect-dlls.sh`：

1. 从构建目录收集可执行文件与插件；
2. 递归解析依赖，仅保留来自 `C:/msys64/ucrt64/bin` 的 DLL；
3. 排除系统 DLL（`kernel32`、`user32`、`ucrtbase`、`vcruntime` 等）；
4. 复制到 `bin/` 并去重；
5. 生成 `VERSION.json` 与 `SHA256SUMS`（由 `package.sh` 在收集后调用）。

### 3.2 Qt 部署

```bash
windeployqt --release --no-translations --no-system-d3d-compiler \
  --dir dist/SRSP-1.0.0-win64/bin \
  dist/SRSP-1.0.0-win64/bin/srsp-studio.exe
```

- 保留 `platforms/qwindows.dll`、`styles/`、`imageformats/`、`iconengines/`；
- **保留 `opengl32sw.dll`**：远程桌面/虚拟机无硬件 OpenGL 时的兜底（Qt 会按需加载）；
- 若使用 Vulkan，保留 `vulkan-1.dll`（系统通常自带，可不带）；
- 多语言：仅保留 `zh_CN`、`en` 翻译文件。

### 3.3 安装器

v1 推荐 **Inno Setup 6**（免费、稳定、脚本简单、支持 per-user 与静默安装）：

- 默认安装到 `%LOCALAPPDATA%\Programs\SRSP`（无需管理员）；
- 可选"为所有用户安装"到 `%ProgramFiles%\SRSP`；
- 开始菜单、桌面快捷方式、`.srproj` 文件关联；
- 卸载保留用户配置（可选清理）；
- 静默参数 `/SILENT /NORESTART`，便于批量部署。

企业需要 MSI 时再引入 WiX（CMake 3.27+ 支持 WiX 3/4 的 CPack 生成器），二者共用同一文件布局。

### 3.4 代码签名

```bat
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 ^
  /f cert.pfx /p %CERT_PASS% bin\srsp-studio.exe
```

- 签名对象：主程序、CLI、核心 DLL、安装器；
- 时间戳必须；EV 证书可减少 SmartScreen 警告；
- 插件包单独签名（见 §5）。

---

## 4. Linux 打包

### 4.1 构建基线

- 在 **Ubuntu 22.04** 容器中构建（glibc 2.35），保证在 22.04/24.04/Debian 12 上可运行；
- 不在滚动发行版上构建发布物（glibc 过新）；
- 链接系统 OpenGL/X11/Vulkan，不打包驱动。

### 4.2 AppImage

```bash
cmake --install "$SRSP_AI_MODEL/Temp/build/linux-gcc-release" --prefix AppDir/usr
linuxdeploy --appdir AppDir \
  --executable AppDir/usr/bin/srsp-studio \
  --desktop-file srsp.desktop \
  --icon-file srsp.png \
  --plugin qt
appimagetool AppDir SRSP-1.0.0-x86_64.AppImage
```

- 需打包 Qt 库与 xcb 相关库；`--plugin qt` 自动处理；
- 验证：在干净容器（`ubuntu:22.04`）中以 `--appimage-extract-and-run` 跑冒烟场景。

### 4.3 deb

CPack DEB 生成器，`CPACK_DEBIAN_PACKAGE_DEPENDS` 声明：

```
libc6 (>= 2.35), libstdc++6, libgl1, libx11-6, libxcb1,
libxkbcommon-x11-0, libvulkan1 | mesa-vulkan-drivers
```

安装路径走 FHS；`postinst` 更新桌面数据库与 MIME。

### 4.4 无头服务器

`srsp-cli` + `srspd`（后续）以 `tar.gz` 交付，不依赖 Qt；渲染用 `renderer.null`；数据目录挂载只读共享盘。

---

## 5. 插件包 `.srpkg`

### 5.1 格式

`.srpkg` 是 zip 容器（存储模式或 deflate），扩展名仅用于识别：

```
org.srsp.env.spice-0.3.1.srpkg
├─ plugin.json
├─ bin/
│  ├─ windows-x86_64/srsp_env_spice.dll
│  └─ linux-x86_64/libsrsp_env_spice.so
├─ data/
│  └─ kernels/README.md          # 大文件不放包内，按需下载
├─ schema/
│  └─ config.schema.json
├─ LICENSE
├─ THIRD_PARTY_NOTICES.md
└─ META-INF/
   ├─ checksums.json             # 每个文件的 sha256
   └─ signature.minisig          # 对 checksums.json 的签名
```

### 5.2 安装

```bash
srsp-cli plugin install ./org.srsp.env.spice-0.3.1.srpkg
srsp-cli plugin list
srsp-cli plugin enable org.srsp.env.spice
srsp-cli plugin rollback org.srsp.env.spice
```

- 安装目录：`%LOCALAPPDATA%\SRSP\plugins\<id>\<version>\`；活跃版本记录在 `installed.json`；
- 校验：解压前验证签名 → 逐文件校验 sha256 → 校验 manifest 与二进制 ABI；
- 失败回滚：安装到临时目录，成功后原子替换；
- 保留最近 2 个版本用于回滚。

### 5.3 签名与信任

- 使用 **minisign**（Ed25519）对 `checksums.json` 签名；
- 平台内置官方公钥；用户可添加第三方发布者公钥；
- 未签名插件只在 `--allow-unsigned` 下可加载，并在 UI 中持续告警；
- 发布者密钥泄露的吊销列表随平台更新分发。

---

## 6. 版本、兼容与升级

| 维度 | 规则 |
|---|---|
| 平台版本 | SemVer；1.x 保持 ABI 1.x 兼容 |
| 场景/项目文件 | 旧版本可打开新文件时提示；新版本自动迁移旧文件并备份 |
| 插件 ABI | 主版本必须匹配；次版本向后兼容 |
| 插件依赖 | 安装时解析能力区间，冲突则拒绝并给出可选组合 |
| 数据包 | 带版本与哈希；不兼容时提示更新插件或数据 |

升级策略：

- 平台升级不覆盖用户插件目录；
- 升级前自动备份 `installed.json` 与设置；
- 大版本升级提供 `srsp-cli doctor` 检查插件兼容性并列出替代实现。

---

## 7. 用户数据目录

| 内容 | Windows | Linux |
|---|---|---|
| 配置 | `%APPDATA%\SRSP\settings.json` | `~/.config/srsp/` |
| 缓存 | `%LOCALAPPDATA%\SRSP\cache\` | `~/.cache/srsp/` |
| 插件 | `%LOCALAPPDATA%\SRSP\plugins\` | `~/.local/share/srsp/plugins/` |
| 日志 | `%LOCALAPPDATA%\SRSP\logs\` | `~/.local/state/srsp/logs/` |
| 项目 | 用户自选（默认 `Documents\SRSP\`） | 用户自选 |
| 录制 | 项目内 `runs/` | 项目内 `runs/` |

原则：程序目录只读；所有可写数据走用户目录；便携模式（`--portable`）将数据放在程序目录旁的 `userdata/`。

---

## 8. 构建产物生成（CMake/CPack）

```cmake
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "SRSP")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "SRSP Team")
set(CPACK_PACKAGE_CONTACT "dev@example.org")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ;DEB")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SRSP Team <dev@example.org>")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.35), libstdc++6, libgl1, libx11-6")
endif()
include(CPack)
```

流程脚本 `tools/packaging/package.sh`（随 M3 提供）：

1. Release 构建 + 测试；
2. `cmake --install` 到 staging；
3. 收集运行库（Windows）/ `linuxdeploy`（Linux）；
4. 生成 `VERSION.json`、`SHA256SUMS`、`sbom.json`；
5. 生成 zip / AppImage / deb / 安装器；
6. 在干净环境跑冒烟测试（见 §10）。

---

## 9. 体积预算

| 组成 | 预算 |
|---|---|
| 内核 + 官方插件 | 80 MB |
| Qt6（Core/Gui/Widgets/Quick/Quick3D） | 120 MB |
| 地球纹理（8K + 夜灯 + 法线） | 150 MB |
| 星表、字体、文档 | 30 MB |
| 安装器压缩后 | ≤ 250 MB |
| 安装后 | ≤ 600 MB |

高精度星历（DE440，约 120 MB）与 30 m 地球瓦片作为**可选数据包**，不进入基础安装器。

---

## 10. 发布前冒烟测试

在**干净虚拟机/容器**（无 MSYS2、无开发工具、断网）中执行：

- [ ] 安装器可完成安装/卸载，快捷方式与文件关联正确
- [ ] 首次启动无缺失 DLL/插件报错，版本信息正确
- [ ] 打开示例项目，播放 60 s，帧率达标
- [ ] 切换 2 个动力学插件，结果一致（容差内）
- [ ] 录制 10 s 并回放，状态哈希一致
- [ ] 相机传感器出图，深度/分割通道正常
- [ ] 插件管理器可安装/禁用/卸载 `.srpkg`
- [ ] 中文界面与高 DPI 正常
- [ ] 卸载后用户数据按预期保留/清理

自动化：`tools/packaging/smoke-test.ps1` / `.sh`（随 M3 提供）驱动 CLI 无头用例，UI 用例手工执行。

---

## 11. 发布流程

```
tag v1.0.0
  └─ CI: 构建矩阵（win-ucrt64 / linux-gcc）→ 测试 → 打包 → 签名 → 生成 SBOM
       └─ 制品：zip、安装器、AppImage、deb、符号包、SHA256SUMS
            └─ 干净环境冒烟
                 └─ 发布页 + 变更日志 + 已知问题
```

- 发布分支保护；制品只从 tag 构建；
- 每个制品记录可复现信息（编译器、依赖版本、源码 commit）；
- 符号文件单独归档并保留至少 2 个版本，用于崩溃回溯。
