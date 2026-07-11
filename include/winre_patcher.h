#pragma once

#include "disk_detector.h"

#include <string>

/**
 * @brief Wipe strategy to apply when WinRE boots.
 *
 *  PartitionWipe  (default)
 *      Targets a single partition by number.
 *      Assigns a temporary drive letter, runs format, removes the letter.
 *      Strategy adapts to disk type:
 *        HDD/Unknown  → format /FS:NTFS /P:1  (1-pass random overwrite)
 *        SSD          → format /FS:NTFS /Q    (quick format + TRIM)
 *      After the wipe the system reboots back into Windows.
 *
 *  FullDiskWipe
 *      Targets the entire physical disk.
 *      Uses diskpart 'clean all' to zero every sector (disk-level operation).
 *      No drive letter assignment required.
 *      Disk type makes no difference — zeros work on both HDD and SSD.
 *      After the wipe the system shuts down (Windows no longer exists).
 */
namespace WinREPatcher
{
    enum class WipeMode
    {
        PartitionWipe,  ///< Wipe one partition (default)
        FullDiskWipe    ///< Zero the entire physical disk (irreversible)
    };

    /**
     * @brief Patches the WinRE image to automatically wipe on the next
     *        WinRE boot according to the selected WipeMode.
     *
     * @param winreDir        WinRE directory path (from WinREFinder).
     * @param diskNumber      Physical disk number (e.g. 0).
     * @param partitionNumber Target partition number — used only in PartitionWipe.
     * @param diskType        Detected disk type   — used only in PartitionWipe.
     * @param mode            Wipe mode (defaults to PartitionWipe).
     * @return true  Patch applied and committed successfully.
     * @return false An error occurred; winre.wim was not modified.
     */
    bool Patch(const std::string&     winreDir,
               int                    diskNumber,
               int                    partitionNumber,
               DiskDetector::DiskType diskType,
               WipeMode               mode = WipeMode::PartitionWipe);
}
