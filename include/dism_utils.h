#pragma once

#include <string>
#include <windows.h>

namespace DISMUtils
{
    /**
     * @brief Runs a command in the current console (output visible) and waits.
     *
     * @param cmd       Command line to execute.
     * @param timeoutMs Maximum wait in milliseconds.
     * @return true  Exited with code 0.
     * @return false Could not start, timed out, or non-zero exit.
     */
    bool RunVisible(const std::string& cmd, DWORD timeoutMs = 120'000);

    /** @brief Builds a temporary path like %TEMP%\winre_mount_<PID>. */
    std::string BuildMountDir();

    /** @brief Best-effort discard of a failed DISM mount and cleanup directory. */
    void DiscardMount(const std::string& mountDir);

    /**
     * @brief Mounts a WIM image using DISM.
     * 
     * @param wimPath Full path to the .wim file.
     * @param mountDir Full path to the directory where it should be mounted.
     * @return true Success
     * @return false Failure
     */
    bool MountImage(const std::string& wimPath, const std::string& mountDir);

    /**
     * @brief Commits changes and unmounts the WIM image using DISM.
     * 
     * @param mountDir Full path to the mounted directory.
     * @return true Success
     * @return false Failure
     */
    bool CommitAndUnmount(const std::string& mountDir);
}
