<!-- Suggested forum title: [Release][Multi-Axis] Motion Bridge + Fallen Doll Mod — TCode, Intiface & Handy (OSR2 / SR6) -->

![PixPin_2026-08-27_03-20-02|690x374](upload://cvGVBFJtcoofbpj2SVQhdvHgOrE.jpeg)
![PixPin_2026-08-27_03-21-04|690x375](upload://xEfgn154cZX6ZGT6s5J4YBJMRUm.jpeg)


## English

This release uses a simple two-part setup for **Operation Lovecraft: Fallen Doll**:

- The **Fallen Doll Mod** runs with the game and writes a compact live motion stream.
- **Motion Bridge** is the new standalone Windows application. It reads that stream, shows the six-axis motion and 3D device preview, and sends output to your hardware.

They are designed to be used together.

Game pages:
https://store.steampowered.com/app/1685960/
https://store.steampowered.com/app/1811180/

### Motion Bridge

- Real-time six-axis `L0/L1/L2/R0/R1/R2` motion for SR6; OSR2 uses `L0`.
- USB TCode, Wi-Fi UDP TCode (default: `tcode.local:8000`), and Intiface Desktop output.
- Compatible with Intiface Central 2.6.
- A separate live SR6/OSR 3D preview, which can stay on top of the game window.
- Per-axis gain, inversion and output-range controls, cross-axis smart limits with an editable two-point curve, hard speed limits, output switches, and configurable return positions. Return positions default to `50%` and are used for stream-loss return, explicit stop, startup safety and disabled axes. Position, Range, preferred interval and learned travel are shown consistently as `0–100%`; each axis has its own invert switch immediately left of the preferred-range button. A selected driver axis can dynamically reduce another axis' range or response speed. Axis cards and the 3D preview show the final protected signal even while hardware output is disarmed. Settings are saved with the portable app.
- Safe startup: output always begins disarmed. On stream loss, motion briefly holds and then returns smoothly to centre.
- English and Simplified Chinese UI.
- Use the matching Mod branch for Fallen Doll Demo or Fallen Doll Playtest.
- The Mod also includes additional support for humanoid actions.

### Install and use

1. Download the latest **Fallen Doll Mod release** for your game version, then close the game.
2. Extract the complete Mod package, then double-click **`Install Mod.cmd`**. The current Playtest package searches the Steam libraries, shows the detected destination, checks runtime-folder write access, installs the Mod, and verifies the required files. If automatic detection is unavailable for the downloaded package, follow the edition-specific manual installation steps below.
3. Download and extract the latest **Motion Bridge** portable ZIP, then start `MotionBridge.exe`.
4. Start Fallen Doll and enter an HAnime. In Motion Bridge, wait until **STREAM** shows **ONLINE**.
5. Open the 3D preview and verify that the motion looks correct before connecting hardware.
6. Choose one output method: **USB**, **Wi-Fi**, or **Intiface**. Check the port/address, set safe per-axis ranges, then explicitly select **ARM OUTPUT**. For Intiface, connect the hardware in Intiface Central first.

#### Manual Mod installation (fallback only)

1. Close Fallen Doll completely and extract the entire downloaded Mod ZIP.
2. Choose the destination that matches the exact game edition:

| Edition | Destination inside the game folder |
|---|---|
| Playtest | `Paralogue\Binaries\Win64` |
| Demo Desktop | `Desktop\WindowsNoEditor\Paralogue\Binaries\Win64` |
| Demo VR | `VR\WindowsNoEditor\Paralogue\Binaries\Win64` |

3. Open the extracted package's `Game` folder. Copy the **contents inside it**—`dwmapi.dll` and the `ue4ss` folder—into the destination above. Do not copy the outer `Game` folder itself. Allow Windows to merge `ue4ss` and replace the packaged Mod files when prompted.
4. Confirm that the selected destination contains these three paths:

```text
<destination>
├─ dwmapi.dll
└─ ue4ss
   ├─ UE4SS.dll
   └─ Mods
      └─ fd_tcode_probe
         └─ Scripts
            └─ main.lua
```

5. Start Motion Bridge, then start Fallen Doll and enter an HAnime. The Mod intentionally adds no in-game menu, overlay, or debug console. A working installation is shown by Motion Bridge changing from **STREAM WAITING** to **STREAM ONLINE** after fresh motion frames arrive.
6. If the stream remains waiting, open `<destination>\ue4ss\UE4SS.log` and check whether `%USERPROFILE%\.f8\studio\games\fallen-doll\runtime\fd-skeleton.ndjson` exists and continues updating during the HAnime.

Do not arm output until the preview is correct. Stop/disarm the app before changing hardware or testing a new pose.

If you encounter a problem, please leave a reply in this topic with your game version, the scene/action, and any relevant error details.

### Troubleshooting — STREAM WAITING

MotionBridge and the Mod do not make a network connection to each other. The Mod writes a local motion file and MotionBridge reads the same file:

`%USERPROFILE%\.f8\studio\games\fallen-doll\runtime\fd-skeleton.ndjson`

**STREAM WAITING** means that MotionBridge has not received a new valid bone frame recently. Check these points in order:

1. Use the Mod release matching the exact game edition. Do not assume the repository's Latest release matches Demo, Playtest, or another installation.
2. Confirm the Mod is installed beside the game executable using the edition-specific destination table above. A working destination contains `dwmapi.dll`, `ue4ss\UE4SS.dll`, and `ue4ss\Mods\fd_tcode_probe\Scripts\main.lua`.
3. Open `<destination>\ue4ss\UE4SS.log` after starting the game. This distinguishes UE4SS not injecting from the Lua Mod not loading.
4. Enter a recognised HAnime, then check whether `fd-skeleton.ndjson` exists and its modified time continues to change while the action is running.
5. Check for old `F8STUDIO_GAMES_DIR` or `FD_TCODE_RUNTIME_DIR` environment variables. Either one can deliberately redirect the Mod to a different runtime folder.

When reporting this issue, include the game edition and version, the complete downloaded ZIP filename, the relevant UE4SS.log lines, and whether the motion file exists and updates during an HAnime.

### Downloads — always use the latest release

- **Motion Bridge (Windows x64 portable):** [latest release and download](https://github.com/Huarch/MotionBridge/releases/latest)
- **Fallen Doll Mod:** [all current releases and downloads](https://github.com/Huarch/MotionBridge-GameMod/releases) — download the newest package labelled **Demo** or **Playtest** for your game version.
- **Discord community:** [join the MotionBridge Discord](https://discord.gg/wc8mn4ejsz)

The relevant SHA-256 is published on each Release page. Verify it against the package you download.

### Discord community

For ongoing support and release discussion, [join the MotionBridge Discord](https://discord.gg/wc8mn4ejsz), then use these channels:

- [Announcements](https://discord.com/channels/1543526971910596649/1543529702558269470) and [download / install help](https://discord.com/channels/1543526971910596649/1543529706769354752)
- [Support and troubleshooting](https://discord.com/channels/1543526971910596649/1543529735399809036)
- [Bug reports](https://discord.com/channels/1543526971910596649/1543529729993482300) and [feature requests](https://discord.com/channels/1543526971910596649/1543529732015136838)
- [English community](https://discord.com/channels/1543526971910596649/1543529710267539496), [中文社区](https://discord.com/channels/1543526971910596649/1543529720413429910), and [showcase](https://discord.com/channels/1543526971910596649/1543546254787612752)

### Notes and limitations

- This is an unofficial community project; it is not affiliated with the game developers or hardware vendors.
- The packages do not include game assets, device drivers, or Intiface Desktop.
- Intiface support uses the first declared **Position-with-duration** or plain **Position** feature and maps it from `L0`; it is not a generic SR6 mapping.
- The persistent **Auto reconnect** status button in the Device connection header applies to USB, Wi-Fi and Intiface. It retries detected transport failures with a 0.5–5 second backoff and resumes only output that was already enabled. USB retries a lost serial port; Intiface retries server or device disconnects; Wi-Fi rebuilds output after local network, address-resolution or UDP socket errors. UDP cannot confirm whether the remote device itself is connected. **STOP OUTPUT** or changing transport always cancels reconnection.
- Intiface declares each device's output feature and native Value range. Motion Bridge maps the unified L0 `0–100%` signal into that range automatically: TCode uses `0–9999`, Handy advertises `0–100`, and other devices use their own declared limits. The L0 Range remains entirely user-controlled.
- For timed-position linear devices, mainly Handy, Motion Bridge retains only the newest L0 target and sends it from a dedicated precise `20 Hz` timer. **Target arrival time** defaults to Automatic, where each `Duration` follows the real timer interval (normally `50 ms`, automatically adjusted after a delayed tick). Advanced users may select `50–100 ms` manually in `5 ms` steps; `50–70 ms` is the practical tuning range, while higher values add more response delay. Ordinary **Position** remains the fallback; USB/Wi-Fi TCode and vibration output are unaffected.
- New or unusual scenes should always be checked in the preview first. Set conservative output ranges for your hardware.

---
## 中文

这次更新采用两个组件配合使用：

- **Fallen Doll Mod** 随游戏运行，输出精简的实时动作数据。
- **Motion Bridge** 是新的独立 Windows 软件，读取动作数据，提供六轴数值与 3D 设备预览，并将信号发送到设备。

两者需要配合使用。

游戏页面：
https://store.steampowered.com/app/1685960/
https://store.steampowered.com/app/1811180/

### Motion Bridge

- SR6 实时六轴 `L0/L1/L2/R0/R1/R2`；OSR2 使用 `L0`。
- 支持 USB TCode、Wi-Fi UDP TCode（默认 `tcode.local:8000`）和 Intiface Desktop 输出。
- 兼容 Intiface Central 2.6。
- 独立的 SR6/OSR 实时 3D 预览窗口，可置顶显示在游戏上方。
- 每轴增益、反向与输出范围、带双点曲线的跨轴智能限制、硬性速度限制、输出开关和归中位置设置；每轴归中位置默认均为 `50%`，并用于断流归中、主动停止、启动安全位置和已关闭轴。位置、Range、偏好区间与学习行程统一显示为 `0–100%`，每张轴卡的偏好区间按钮左侧都有独立反向开关。智能限制可根据所选驱动轴的位置，动态缩小另一个轴的行程或响应速度。即使硬件输出未解锁，轴卡和 3D 预览也会显示最终处理后的信号。便携版会保存设置。
- 安全启动：设备输出默认未解锁。数据流中断时会短暂停留，然后平滑回中。
- 支持英文和简体中文界面。
- 请按游戏版本选择 Fallen Doll Demo 或 Fallen Doll Playtest 对应的 Mod 分支。
- Mod 还额外支持类人动作。

### 安装与使用

1. 根据游戏版本下载最新的 **Fallen Doll Mod 发布包**，然后关闭游戏。
2. 完整解压 Mod 包，然后双击 **`Install Mod.cmd`**。当前 Playtest 安装包会自动搜索 Steam 库，显示检测到的目标位置，检查 runtime 目录写入权限，安装 Mod，并验证必需文件。如果下载的版本尚不提供自动检测，请按照下方区分版本的步骤手动安装。
3. 下载并解压最新版 **Motion Bridge** 便携包，然后启动 `MotionBridge.exe`。
4. 启动 Fallen Doll 并进入 HAnime。在 Motion Bridge 中等待 **STREAM** 显示为 **ONLINE**。
5. 打开 3D 预览，先确认动作正确，再连接设备。
6. 选择一种输出方式：**USB**、**Wi-Fi** 或 **Intiface**。确认端口/地址，为设备设置安全的各轴范围后，再手动点击 **ARM OUTPUT**。使用 Intiface 时，先在 Intiface Central 中连接设备。

#### 手动安装 Mod（仅作备用）

1. 完全关闭 Fallen Doll，并完整解压下载的 Mod ZIP。
2. 根据实际游戏版本选择目标目录：

| 游戏版本 | 游戏目录内的目标路径 |
|---|---|
| Playtest | `Paralogue\Binaries\Win64` |
| Demo Desktop | `Desktop\WindowsNoEditor\Paralogue\Binaries\Win64` |
| Demo VR | `VR\WindowsNoEditor\Paralogue\Binaries\Win64` |

3. 打开解压包内的 `Game` 文件夹，将其中的**内容**——`dwmapi.dll` 和 `ue4ss` 文件夹——复制到上方对应的目标目录。不要把外层 `Game` 文件夹本身复制进去。Windows 提示时，允许合并 `ue4ss` 并替换安装包提供的 Mod 文件。
4. 确认所选目标目录最终包含以下三个路径：

```text
<目标目录>
├─ dwmapi.dll
└─ ue4ss
   ├─ UE4SS.dll
   └─ Mods
      └─ fd_tcode_probe
         └─ Scripts
            └─ main.lua
```

5. 先启动 Motion Bridge，再启动 Fallen Doll 并进入 HAnime。此 Mod 不会显示游戏内菜单、浮层或调试控制台；收到新动作帧后，Motion Bridge 从 **STREAM WAITING** 变为 **STREAM ONLINE**，才是可见的运行结果。
6. 如果数据流一直处于等待状态，请打开 `<目标目录>\ue4ss\UE4SS.log`，并检查进入 HAnime 后 `%USERPROFILE%\.f8\studio\games\fallen-doll\runtime\fd-skeleton.ndjson` 是否存在且持续更新。

预览未确认正确前，请不要解锁输出。更换硬件或测试新姿势前，请先停止/解除输出。

遇到问题请在本帖留言，并尽量提供游戏版本、场景/动作和相关错误信息。

### 排错 — STREAM WAITING

MotionBridge 和 Mod 之间不是网络连接。Mod 会写入本地动作文件，MotionBridge 读取同一个文件：

`%USERPROFILE%\.f8\studio\games\fallen-doll\runtime\fd-skeleton.ndjson`

**STREAM WAITING** 表示 MotionBridge 最近没有收到新的有效骨骼帧。请按以下顺序检查：

1. 下载与实际游戏版本完全匹配的 Mod 发布包。不要因为它是仓库的 Latest Release 就假定适用于 Demo、Playtest 或另一套安装。
2. 按照上方区分版本的目标目录表，确认 Mod 安装在游戏 EXE 所在目录。正常的目标目录应包含 `dwmapi.dll`、`ue4ss\UE4SS.dll` 和 `ue4ss\Mods\fd_tcode_probe\Scripts\main.lua`。
3. 启动游戏后打开 `<目标目录>\ue4ss\UE4SS.log`。它可区分 UE4SS 未注入与 Lua Mod 未加载两种情况。
4. 进入已识别的 HAnime，再检查 `fd-skeleton.ndjson` 是否存在，以及动作运行期间修改时间是否持续变化。
5. 检查旧的 `F8STUDIO_GAMES_DIR` 或 `FD_TCODE_RUNTIME_DIR` 环境变量；它们会让 Mod 有意写入另一个运行目录。

反馈该问题时，请附上游戏版本、下载 ZIP 的完整文件名、相关 UE4SS.log 内容，以及 HAnime 运行时动作文件是否存在并持续更新。

### 下载 — 始终使用最新 Release

- **Motion Bridge（Windows x64 便携版）：** [最新 Release 与下载](https://github.com/Huarch/MotionBridge/releases/latest)
- **Fallen Doll Mod：** [当前全部 Release 与下载](https://github.com/Huarch/MotionBridge-GameMod/releases) —— 按游戏版本下载名称标有 **Demo** 或 **Playtest** 的最新包。
- **Discord 社区：** [加入 MotionBridge Discord](https://discord.gg/wc8mn4ejsz)

每个 Release 页面都会发布对应的 SHA-256；请按实际下载的包进行校验。

### Discord 社区

请先[加入 MotionBridge Discord](https://discord.gg/wc8mn4ejsz)，再按问题类型进入对应频道：

- [公告](https://discord.com/channels/1543526971910596649/1543529702558269470) 与 [下载 / 安装求助](https://discord.com/channels/1543526971910596649/1543529706769354752)
- [支持与排错](https://discord.com/channels/1543526971910596649/1543529735399809036)
- [Bug 反馈](https://discord.com/channels/1543526971910596649/1543529729993482300) 与 [功能建议](https://discord.com/channels/1543526971910596649/1543529732015136838)
- [英文社区](https://discord.com/channels/1543526971910596649/1543529710267539496)、[中文社区](https://discord.com/channels/1543526971910596649/1543529720413429910) 与 [展示区](https://discord.com/channels/1543526971910596649/1543546254787612752)

### 说明与限制

- 这是非官方社区项目，与游戏开发商和设备厂商没有关联。
- 发布包不包含游戏资源、设备驱动或 Intiface Desktop。
- Intiface 当前使用设备声明的第一个 **Position-with-duration** 或普通 **Position** 功能，并由 `L0` 驱动；它不是通用的 SR6 映射。
- “设备连接”标题区的“自动重连”状态按钮同时适用于 USB、Wi-Fi 和 Intiface，并会持久保存。开启后，检测到连接故障时会按 0.5–5 秒退避重试，并且只恢复之前已启用的输出。USB 会重试丢失的串口；Intiface 会重连服务器或设备；Wi-Fi 会在本机网络、地址解析或 UDP 套接字报错后重建输出。UDP 无法确认远端设备本身是否在线。手动停止输出或切换连接方式会立即取消重连。
- Intiface 会声明各设备的输出功能和原生 Value 范围。Motion Bridge 会把统一的 L0 `0–100%` 自动映射到该范围：TCode 使用 `0–9999`，Handy 声明 `0–100`，其他设备使用各自声明的范围；L0 Range 始终由用户自行设置。
- 对支持 **Position-with-duration** 的线性设备（主要是 Handy），Motion Bridge 只保留最新 L0 目标，并通过独立的精确 `20 Hz` 定时器发送。“目标到达时间”默认使用“自动”，此时每条命令的 `Duration` 跟随真实定时周期（通常为 `50 ms`，定时器延迟时自动调整）。高级用户可以按 `5 ms` 步进手动选择 `50–100 ms`；`50–70 ms` 是较实用的范围，更高数值会增加响应延迟。普通 **Position** 仍作为回退；USB/Wi-Fi TCode 和振动输出不受影响。
- 遇到新动作或特殊姿势，请先在预览中确认，并为自己的设备使用保守的输出范围。
