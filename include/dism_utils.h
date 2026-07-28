#pragma once

#include <string>
#include <windows.h>

/** @brief DISM (Deployment Imaging Service and Management) wrapper utilities. */
namespace DISMUtils
{
    /** @brief Runs a command in the current console (output visible) and waits for it. */
    bool RunVisible(const std::string& cmd, DWORD timeoutMs = 120'000);

    /** @brief Builds a temporary path like %TEMP%\winre_mount_<PID>. */
    std::string BuildMountDir();

    /** @brief Best-effort discard of a failed DISM mount and cleanup directory. */
    void DiscardMount(const std::string& mountDir);

    /** @brief Mounts a WIM image using DISM. */
    bool MountImage(const std::string& wimPath, const std::string& mountDir);

    /** @brief Commits changes and unmounts the WIM image using DISM. */
    bool CommitAndUnmount(const std::string& mountDir);
}
