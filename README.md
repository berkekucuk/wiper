# winre-boot

A C++17 console application that locates the Windows Recovery Environment (WinRE),
patches its boot script to open a terminal running a specified command, then reboots
the system directly into that shell.

## Features

- Detects WinRE location via `reagentc /info` with Registry fallback
- Mounts `winre.wim` via DISM and patches `startnet.cmd` to inject a `cmd.exe` shell
- The injected command (e.g. `diskpart`) runs automatically on WinRE boot
- After closing the shell, execution falls through to the standard WinRE GUI
- Runs a **5-second countdown** that can be cancelled by pressing any key
- Reboots into WinRE via `reagentc /boottore`

## Requirements

| Tool | Version |
|------|---------|
| Windows | 10 / 11 |
| [w64devkit](https://github.com/skeeto/w64devkit) | 1.x (GCC 13+) |
| CMake | 3.20 or later |
| Ninja | bundled with w64devkit |

> **Important:** The program must be run as **Administrator** (required by DISM and `reagentc`).

## Build

Open the w64devkit terminal **as Administrator** and navigate to the project directory:

```sh
# Configure (Ninja generator, GCC)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build
```

Output: `build\winre-boot.exe`

## Usage

```
winre-boot.exe
```

The application performs the following steps:

1. Locates WinRE and prints its path and status
2. Mounts `winre.wim` and patches `startnet.cmd` to inject `cmd.exe /k <command>`
3. Runs a 5-second countdown (press any key to cancel)
4. Reboots the system into WinRE → terminal opens automatically

## Changing the Command

Edit the constant at the top of [`src/main.cpp`](src/main.cpp):

```cpp
constexpr const char* kTestCommand = "diskpart";
```

Replace `"diskpart"` with any command you want to run in the WinRE shell, e.g.:
- `"sfc /scannow"` — system file checker
- `"chkdsk C: /f"` — disk check
- `"cmd"` — plain interactive shell

## Project Structure

```
winre-boot/
├── include/
│   ├── winre_finder.h    # WinRE locator interface
│   ├── countdown.h       # Countdown interface
│   ├── reboot.h          # Reboot interface
│   └── winre_patcher.h   # WinRE image patcher interface
├── src/
│   ├── main.cpp          # Entry point (kTestCommand lives here)
│   ├── winre_finder.cpp  # WinRE detection (reagentc + Registry)
│   ├── countdown.cpp     # Countdown timer with key detection
│   ├── reboot.cpp        # reagentc /boottore + ExitWindowsEx
│   └── winre_patcher.cpp # DISM mount → startnet.cmd patch → commit
└── CMakeLists.txt        # Build system
```

## Technical Notes

- **WinRE detection:** `reagentc /info` output is parsed first; if that fails,
  `HKLM\SOFTWARE\Microsoft\RecoveryEnvironment` is read from the Registry.
- **Patching:** DISM mounts `winre.wim` at `%TEMP%\winre_mount_<PID>`,
  `cmd.exe /k <command>` is prepended to `startnet.cmd`, then the image is committed.
- **startnet.cmd flow:** injected command → user closes shell → `wpeinit` → WinRE GUI.
- **Modification is permanent.** There is no undo. If DISM fails mid-operation,
  run `dism /Cleanup-Mountpoints` to clean up stale mounts.
- **Key detection:** `_kbhit()` with 100 ms polling; 1 second = 10 polling cycles.
- **Shutdown privilege:** `SE_SHUTDOWN_NAME` is granted via `AdjustTokenPrivileges`.
- **Restart:** `ExitWindowsEx(EWX_REBOOT | EWX_FORCE)` is attempted first;
  `InitiateSystemShutdownExA` is used as a fallback.
