# winre-boot

A C++17 console application that locates the Windows Recovery Environment (WinRE) and reboots the system into it.

## Features

- Detects WinRE location via `reagentc /info` and falls back to the Registry
- Prints the path and status to the terminal
- Runs a **5-second countdown** that can be cancelled by pressing any key
- Sets the one-time WinRE boot flag via `reagentc /boottore`
- Restarts the system into WinRE

## Requirements

| Tool | Version |
|------|---------|
| Windows | 10 / 11 |
| [w64devkit](https://github.com/skeeto/w64devkit) | 1.x (GCC 13+) |
| CMake | 3.20 or later |
| Ninja | bundled with w64devkit |

> **Important:** The program must be run as **Administrator**.

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
2. Starts a 5-second countdown (press any key to cancel)
3. Reboots the system into WinRE

## Project Structure

```
winre-boot/
├── include/
│   ├── winre_finder.h   # WinRE locator interface
│   ├── countdown.h      # Countdown interface
│   └── reboot.h         # Reboot interface
├── src/
│   ├── main.cpp         # Entry point
│   ├── winre_finder.cpp # WinRE detection (reagentc + Registry)
│   ├── countdown.cpp    # Countdown timer with key detection
│   └── reboot.cpp       # reagentc /boottore + ExitWindowsEx
└── CMakeLists.txt       # Build system
```

## Technical Notes

- **WinRE detection:** `reagentc /info` output is parsed first; if that fails, `HKLM\SOFTWARE\Microsoft\RecoveryEnvironment` is read from the Registry.
- **Key detection:** `_kbhit()` with 100 ms polling; 1 second = 10 polling cycles.
- **Shutdown privilege:** `SE_SHUTDOWN_NAME` is granted via `AdjustTokenPrivileges`.
- **Restart:** `ExitWindowsEx(EWX_REBOOT | EWX_FORCE)` is attempted first; `InitiateSystemShutdownExA` is used as a fallback.
