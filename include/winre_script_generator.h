#pragma once

#include <string>
#include "disk_detector.h"

namespace WinREScriptGenerator
{


    /**
     * @brief Embeds all necessary scripts for a FullDiskWipe into the mounted WinRE image.
     */
    bool GenerateFullDiskWipeScripts(const std::string& mountDir, int diskNumber, bool isSSD);

    /**
     * @brief Updates winpeshl.ini inside the mounted WinRE image to launch
     *        wipe_launch.cmd instead of the standard recovery UI.
     */
    bool PatchWinpeshl(const std::string& mountDir);
}
