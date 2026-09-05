# Motion Bridge

[简体中文](README-ZH.md)

Native Windows bridge that converts real-time game motion into multi-axis device output. Fallen Doll is the first bundled game adapter, while the public `motion-frame/v1` input keeps the application open to additional games. The core is C++20 and does not depend on Qt; the desktop application uses Qt 6/QML.

Motion Bridge has one real-time worker thread for file watching, motion calculation and output. QML only renders copied snapshots at its own cadence; it never blocks the device path.

## Current features

- Real-time L0/L1/L2/R0/R1/R2 processing for OSR2/SR6 workflows.
- USB serial, Wi-Fi UDP (`tcode.local:8000` by default), and Intiface Desktop output.
- A transport-independent fixed-rate output clock with measured TCode intervals, soft start, cross-axis smart limits, per-axis speed limits, output switches, configurable per-axis return positions, and an optional global safety-distance gate. Return positions default to `50%` and are shared by stream-loss return, explicit stop, startup safety and disabled axes. Far-away initialization poses cannot enter six-axis processing or preferred-range learning until Reference and Target are close enough. Axis cards and the 3D preview always follow the final protected signal, whether hardware output is armed or not.
- Separate SR6/OSR 3D viewer with optional always-on-top mode.
- Per-axis gain (output travel around the selected center), inversion, and output-range controls, plus an independent preferred-range optimizer for every axis. Position and range values are shown consistently as `0–100%`, with 0.1% range adjustment. The optimizer maps learned stable endpoints into a configurable minimum–maximum interval, enlarges only short travel, and leaves extra-motion headroom; translational axes are capped at 4× automatic gain and rotational axes at a conservative 2×. Settings are saved beside the executable in portable mode.
- Light and dark themes, plus English, Simplified Chinese, and system-language selection.
- A per-app display-scale selector (system, 75%, 90%, 100%, 110%, or 125%) applies on the next launch, so high-DPI desktops can keep MotionBridge compact.
- Safe startup, stream-loss hold and smooth return to center. Device output always starts disarmed.

## Quick start with Fallen Doll

1. Install the game-side Mod from [MotionBridge-FallenDoll](https://github.com/Huarch/MotionBridge-FallenDoll).
2. Extract the Motion Bridge portable ZIP and run `MotionBridge.exe`.
3. Enter an HAnime and wait for the stream status to show **ONLINE**.
4. Open the separate 3D preview and check the motion before connecting hardware.
5. Select USB, Wi-Fi, or Intiface, then explicitly choose **ARM OUTPUT**.
   - For Intiface devices, connect the hardware in Intiface Central first. Motion Bridge maps the user-selected L0 `0–100%` Range to the Value range advertised by each device.

The compatibility stream is currently read from `%USERPROFILE%/.f8/studio/games/fallen-doll/runtime/fd-skeleton.ndjson`; F8Studio, `fd_source`, and `fd_pyengine` do not need to be running.

## Development

Build the deterministic core and tests without Qt:

```powershell
cmake -S . -B out/core -DMOTION_BRIDGE_BUILD_GUI=OFF
cmake --build out/core --config Release
ctest --test-dir out/core -C Release --output-on-failure
```

For the desktop application, install Qt 6.8+ with Core, Quick, Quick3D, SerialPort, and WebSockets, then configure with `MOTION_BRIDGE_BUILD_GUI=ON`.

The repository contains two Windows helpers:

```powershell
# One-time development toolchain under .toolchain/qt
.\tools\Install-MotionBridgeToolchain.ps1

# Deterministic core only (does not require Qt)
.\tools\Build-MotionBridge.ps1 -CoreOnly

# Desktop application after Qt installation
.\tools\Build-MotionBridge.ps1 -QtPrefix D:\path\to\Qt\6.8.3\msvc2022_64
```

The game-side UE4SS Mod remains the source of compact functional-bone frames. This project consumes its `fd-skeleton.ndjson` output directly, so F8Studio does not need to be running. File watching is backed by a 50 Hz incremental read to remain reliable while Unreal replaces or keeps the stream file open.

The chosen stream file also accepts the public `motion-frame/v1` NDJSON format in `adapters/`. This is the supported extension point for future games: an adapter writes a completed frame per line. Motion Bridge does not load arbitrary third-party DLLs.

## Portable build

After installing the isolated Qt/MinGW toolchain, create a tested portable directory and ZIP:

```powershell
.\tools\Build-MotionBridge.ps1 -Portable
```

It uses `windeployqt` to copy the Qt runtime beside the executable. The output stays under the ignored `dist/` folder.

## Output safety

- USB sends TCode at 115200 baud.
- Wi-Fi uses the same UDP TCode transport as the existing F8Studio project (`tcode.local:8000` by default).
- The persistent **Auto reconnect** status button in the Device connection header applies to USB, Wi-Fi and Intiface. It retries detected transport failures with a 0.5–5 second backoff and resumes only output that the user had already enabled. USB retries a lost serial port; Intiface retries server or device disconnects; Wi-Fi rebuilds its output after local network, address-resolution or UDP socket errors. Because UDP has no acknowledgement, Motion Bridge cannot confirm whether the remote Wi-Fi device itself is connected. **STOP OUTPUT** or changing transport always cancels reconnection.
- Intiface connects to the user's Intiface Desktop WebSocket at `ws://127.0.0.1:12345`. It enables the first declared timed-position or plain Position feature and maps L0 to it; it does not pretend that a generic Intiface device is an SR6.
- Intiface advertises the supported output feature and its native Value range. Motion Bridge maps the unified L0 `0–100%` signal into that range automatically: TCode remains `0–9999`, Handy reports `0–100`, and other devices use their own declared limits. The user controls the L0 Range directly; selecting a device never rewrites it.
- For timed-position linear devices, mainly Handy, Motion Bridge prefers `HwPositionWithDuration` and retains only the newest L0 target. A dedicated precise timer sends at `20 Hz`. **Target arrival time** defaults to Automatic, where each command `Duration` follows the real timer interval (normally `50 ms`, automatically adjusted after a delayed tick). Advanced users may disable Automatic and select `50–100 ms` in `5 ms` steps; `50–70 ms` is the practical tuning range, while higher values trade response for softer motion. Plain `Position` remains the fallback for devices without timed position; USB/Wi-Fi TCode and vibration output are unaffected.
- A start or imported configuration is always disarmed. Stream loss holds the final value for 250 ms, then returns to center over 600 ms.
- Advanced output processing defaults to 50 Hz with a 600 ms soft start. Intiface timed-position scheduling uses its own 20 Hz clock and defaults to automatic target arrival timing, independently of this processing rate. Each axis card opens compact smart-limit and protection popups. A selected driver axis controls how much range or response speed the target axis retains through two freely movable curve points. Each axis can also be hard speed-limited or disabled independently, and returns to its configurable position after stream loss; the default is `50%`.

## One-time F8Studio migration

The following reads the saved Fallen Doll project from `%USERPROFILE%\.f8\studio\assets.db` and writes `%LOCALAPPDATA%\MotionBridge\motion-bridge.ini`. It imports connection addresses, range and tuning values, but always leaves output disarmed.

```powershell
python .\tools\Import-F8StudioSettings.py
```
