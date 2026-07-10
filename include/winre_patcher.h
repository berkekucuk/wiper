#pragma once

#include <string>

/**
 * @brief Module that patches a WinRE image (winre.wim) to automatically launch
 *        a cmd.exe terminal running a specified command on the next WinRE boot.
 *
 * The patch is applied by:
 *   1. Mounting winre.wim via DISM.
 *   2. Prepending `cmd.exe /k <command>` to startnet.cmd inside the image.
 *   3. Committing and unmounting the image.
 *
 * The modification is permanent (no backup / no undo).
 */
namespace WinREPatcher
{
    /**
     * @brief Patches the WinRE image so that on the next WinRE boot, the given
     *        command is executed automatically in a cmd.exe terminal.
     *        After the user closes the shell, execution continues to wpeinit and
     *        the standard WinRE GUI (as a fallback).
     *
     * @param winreDir  Path to the WinRE directory (as returned by WinREFinder).
     *                  "winre.wim" will be appended automatically if needed.
     * @param command   Command to inject, e.g. "diskpart" or "sfc /scannow".
     * @return true  Patch applied and committed successfully.
     * @return false An error occurred; winre.wim was not modified (or was discarded).
     */
    bool Patch(const std::string& winreDir, const std::string& command);
}
