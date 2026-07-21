#pragma once

#include <string>

/**
 * @brief Functions for locating the Windows Recovery Environment (WinRE).
 */
namespace WinREFinder
{
    /**
     * @brief Resolves the WinRE installation path.
     *        First detects the locale-independent WinRE device path in the
     *        output of `reagentc /info`;
     *        falls back to reading the Registry if that fails.
     *
     * @param outPath   Receives the resolved WinRE path on success.
     * @param outStatus Receives "Detected" when the WinRE device path is found.
     * @return true  Path was resolved successfully.
     * @return false Path could not be determined.
     */
    bool FindLocation(std::string& outPath, std::string& outStatus);

    /**
     * @brief Prints WinRE information to the console in a formatted box.
     *
     * @param path   WinRE path.
     * @param status WinRE status.
     */
    void PrintInfo(const std::string& path, const std::string& status);
}
