#include "wim_resolver.h"

#include <windows.h>
#include <iostream>

using namespace std;

namespace
{
    /** @brief Scans A-Z for <drive>:\Recovery\WindowsRE\winre.wim. */
    string ScanDrivesForWim()
    {
        for (char d = 'A'; d <= 'Z'; ++d)
        {
            string p;
            p += d;
            p += ":\\Recovery\\WindowsRE\\winre.wim";
            if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES)
                return p;
        }
        return {};
    }
}

namespace WIMResolver
{
    string ResolveWimPath(const string& winreDir)
    {
        auto exists = [](const string& p) {
            return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
        };

        // Already a .wim path?
        constexpr string_view ext = ".wim";
        if (winreDir.size() >= ext.size() &&
            winreDir.compare(winreDir.size() - ext.size(), ext.size(), ext) == 0 &&
            exists(winreDir))
        {
            return winreDir;
        }

        // Append winre.wim to directory
        string candidate = winreDir;
        if (!candidate.empty() && candidate.back() != '\\')
            candidate += '\\';
        candidate += "winre.wim";

        if (exists(candidate))
            return candidate;

        // Fallback: scan drive letters
        cout << "  Primary path not directly accessible; scanning drives...\n";
        return ScanDrivesForWim();
    }
}
