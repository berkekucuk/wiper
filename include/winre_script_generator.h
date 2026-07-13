#pragma once

#include <string>
#include "disk_detector.h"

namespace WinREScriptGenerator
{
    /**
     * @brief Embeds all necessary scripts for a PartitionWipe into the mounted WinRE image.
     */
    bool GeneratePartitionWipeScripts(const std::string& mountDir,
                                      int diskNumber,
                                      int partitionNumber,
                                      DiskDetector::DiskType diskType);

    /**
     * @brief Embeds all necessary scripts for a FullDiskWipe into the mounted WinRE image.
     */
    bool GenerateFullDiskWipeScripts(const std::string& mountDir,
                                     int diskNumber,
                                     DiskDetector::DiskType diskType);

    /**
     * @brief Updates winpeshl.ini inside the mounted WinRE image to launch
     *        wipe_launch.cmd instead of the standard recovery UI.
     */
    bool PatchWinpeshl(const std::string& mountDir);
}
