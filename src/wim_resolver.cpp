#include "wim_resolver.h"
#include <windows.h>

#include <iostream>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /** @brief Scans A-Z for winre.wim on fixed drives only. */
    std::string ScanDrivesForWim()
    {
        for (char d = 'A'; d <= 'Z'; ++d)
        {
            std::string root;
            root += d;
            root += ":\\";

            // Skip non-fixed drives: network shares, CD-ROMs, removable media.
            if (GetDriveTypeA(root.c_str()) != DRIVE_FIXED)
                continue;

            std::string p = root + "Recovery\\WindowsRE\\winre.wim";
            if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES)
                return p;
        }
        return {};
    }
}

// ---------------------------------------------------------------------------
// WIMResolver interface implementation
// ---------------------------------------------------------------------------
namespace WIMResolver
{
    std::string ResolveWimPath(const std::string& winreDir)
    {
        auto exists = [](const std::string& p) {
            return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
        };

        // Already a .wim path?
        constexpr std::string_view ext = ".wim";
        if (winreDir.size() >= ext.size() &&
            winreDir.compare(winreDir.size() - ext.size(), ext.size(), ext) == 0 &&
            exists(winreDir))
        {
            return winreDir;
        }

        // Append winre.wim to directory
        std::string candidate = winreDir;
        if (!candidate.empty() && candidate.back() != '\\')
            candidate += '\\';
        candidate += "winre.wim";

        if (exists(candidate))
            return candidate;

        // Fallback: scan drive letters
        std::cout << "  Primary path not directly accessible; scanning drives...\n";
        return ScanDrivesForWim();
    }
}
