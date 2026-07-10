#pragma once

#include <string>

/**
 * @brief Module that patches a WinRE image (winre.wim) to automatically wipe
 *        a specific disk partition on the next WinRE boot — no user input required.
 *
 * The patch embeds two files inside winre.wim:
 *   - Windows\System32\wipe_script.txt  : diskpart commands
 *   - Windows\System32\wipe_launch.cmd  : wrapper that runs diskpart and reboots
 *
 * winpeshl.ini is also updated to launch wipe_launch.cmd on WinRE start.
 */
namespace WinREPatcher
{
    /**
     * @brief Patches the WinRE image to automatically format the specified
     *        partition with a quick NTFS format on the next WinRE boot.
     *        After the format completes the system reboots automatically.
     *
     * @param winreDir       WinRE directory path (from WinREFinder).
     * @param diskNumber      Target disk number   (e.g. 0).
     * @param partitionNumber Target partition number (e.g. 5).
     * @return true  Patch applied and committed successfully.
     * @return false An error occurred; winre.wim was not modified.
     */
    bool Patch(const std::string& winreDir, int diskNumber, int partitionNumber);
}
