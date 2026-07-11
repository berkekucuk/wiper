#pragma once

#include "disk_detector.h"

#include <string>

/**
 * @brief Module that patches a WinRE image (winre.wim) to automatically wipe
 *        a specific disk partition on the next WinRE boot — no user input required.
 *
 * The wipe strategy is selected based on the detected disk type:
 *   - HDD / Unknown  → format /FS:NTFS /P:1  (one pass of random data)
 *   - SSD (SATA/NVMe)→ format /FS:NTFS /Q    (quick format)
 *                      + diskpart retrim      (TRIM signal to SSD controller)
 *
 * Files embedded inside winre.wim (accessible at X:\ on WinRE boot):
 *   Windows\System32\assign_script.txt  — diskpart: assign letter Z:
 *   Windows\System32\remove_script.txt  — diskpart: remove letter Z:
 *   Windows\System32\retrim_script.txt  — diskpart: retrim (SSD only)
 *   Windows\System32\wipe_launch.cmd    — main orchestrator batch
 *
 * winpeshl.ini is updated to launch wipe_launch.cmd on WinRE start.
 */
namespace WinREPatcher
{
    /**
     * @brief Patches the WinRE image to automatically wipe the specified
     *        partition on the next WinRE boot using the optimal strategy for
     *        the detected disk type.
     *
     * @param winreDir        WinRE directory path (from WinREFinder).
     * @param diskNumber      Physical disk number  (e.g. 0).
     * @param partitionNumber Partition number      (e.g. 5).
     * @param diskType        Detected disk type    (from DiskDetector::Detect).
     * @return true  Patch applied and committed successfully.
     * @return false An error occurred; winre.wim was not modified.
     */
    bool Patch(const std::string&     winreDir,
               int                    diskNumber,
               int                    partitionNumber,
               DiskDetector::DiskType diskType);
}
