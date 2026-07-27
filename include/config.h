#pragma once

#include <string>

/** @brief Configuration loader: reads runtime settings from a text file. */
namespace Config
{
    /** @brief Runtime configuration loaded from a text file. */
    struct WipeConfig
    {
        std::string mode;

        int diskNumber = 0;
        std::string hddStrategy;
        std::string ssdStrategy;

        std::string wipeList;
        std::string fileStrategy;
    };

    /** @brief Loads configuration from a text file (default: wiper_config.txt). */
    bool LoadConfig(WipeConfig& config, const std::string& path = "wiper_config.txt");
}
