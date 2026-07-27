#pragma once

#include <string>
#include <vector>

/** @brief File-wipe operations: secure file/directory erasure with multiple strategies. */
namespace FileWiper
{
    /** @brief File-wipe strategy enumeration. */
    enum class FileWipeStrategy
    {
        ZeroFill,
        RandomFill,
        DOD3
    };

    /** @brief Converts a string to FileWipeStrategy (default: ZeroFill). */
    FileWipeStrategy StrategyFromString(const std::string& s);

    /** @brief Parses a file list from a text file (one path per line). */
    bool ParseFileList(const std::string& listPath, std::vector<std::string>& files);

    /** @brief Wipes a single file or directory and deletes it. */
    bool WipeAndDelete(const std::string& path, FileWipeStrategy strategy);

    /** @brief Wipes all entries listed in a file list. Returns success count. */
    int WipeFiles(const std::string& listPath, FileWipeStrategy strategy);

    /** @brief Runs the full file-wipe routine using the given list and strategy. */
    int RunFileWipe(const std::string& wipeList, FileWipeStrategy strategy);
}
