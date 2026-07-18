#include "winre_script_generator.h"
#include "cli_utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;
using namespace CLIUtils::Colors;

namespace WinREScriptGenerator
{
    // Path prefix for files embedded inside the mounted WinRE image.
    // At boot time WinRE maps its System32 as X:\Windows\System32.
    static constexpr const char* kWinRESys32 = "X:\\Windows\\System32\\";

    bool GenerateDiskWipeScripts(const string& mountDir, int diskNumber, WinREPatcher::WipeStrategy strategy)
    {
        // Validate disk number range before writing any scripts.
        // diskpart accepts 0-127; anything outside is invalid.
        if (diskNumber < 0 || diskNumber > 127)
        {
            cerr << "  [ERROR] Invalid disk number: " << diskNumber
                      << ". Must be in range 0-127.\n";
            return false;
        }
        switch (strategy)
        {
        case WinREPatcher::WipeStrategy::ZeroFill:
            {
                string p = mountDir + "\\Windows\\System32\\dp_zerofill.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open()) return false;
                s << "select disk " << diskNumber << "\r\n";
                s << "clean all\r\n";
                s << "exit\r\n";
            }
            {
                string p = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
                ofstream s(p, ios::trunc);
                if (!s.is_open()) return false;
                s << "@echo off\r\n";
                s << "echo.\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo   WinRE Full Disk Wipe (HDD Zero Fill) ^| Disk " << diskNumber << "\r\n";
                s << "echo   ALL DATA will be permanently erased!\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo.\r\n";
                s << "echo  [1/2] Running diskpart clean all...\r\n";
                s << "echo  This may take 30-60 minutes for large drives.\r\n";
                s << "echo.\r\n";
                s << "diskpart /s " << kWinRESys32 << "dp_zerofill.txt\r\n";
                s << "echo.\r\n";
                s << "echo  [2/2] Disk wiped. Shutting down in 5 seconds...\r\n";
                s << "ping 127.0.0.1 -n 6 > nul\r\n";
                s << "wpeutil shutdown\r\n";
            }
            break;

        case WinREPatcher::WipeStrategy::RandomFill:
            {
                {
                    string p = mountDir + "\\Windows\\System32\\dp_randomfill_prep.txt";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "select disk " << diskNumber << "\r\n";
                    s << "clean\r\n";
                    s << "create partition primary\r\n";
                    s << "assign letter=Z\r\n";
                    s << "exit\r\n";
                }
                {
                    string p = mountDir + "\\Windows\\System32\\dp_randomfill_cleanup.txt";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "select disk " << diskNumber << "\r\n";
                    s << "clean\r\n";
                    s << "exit\r\n";
                }
                {
                    string p = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "@echo off\r\n";
                    s << "echo.\r\n";
                    s << "echo  ====================================================\r\n";
                    s << "echo   WinRE Full Disk Wipe (HDD Random Fill) ^| Disk " << diskNumber << "\r\n";
                    s << "echo   ALL DATA will be overwritten with random bits!\r\n";
                    s << "echo  ====================================================\r\n";
                    s << "echo.\r\n";
                    s << "echo  [1/3] Preparing Disk (removing partitions ^& assigning Z:)...\r\n";
                    s << "diskpart /s " << kWinRESys32 << "dp_randomfill_prep.txt\r\n";
                    s << "echo.\r\n";
                    s << "echo  [2/3] Formatting with random data (/P:1)...\r\n";
                    s << "echo  This may take SEVERAL HOURS for large drives.\r\n";
                    s << "format Z: /FS:NTFS /P:1 /Y\r\n";
                    s << "echo.\r\n";
                    s << "echo  [3/3] Final cleanup (returning disk to RAW state)...\r\n";
                    s << "diskpart /s " << kWinRESys32 << "dp_randomfill_cleanup.txt\r\n";
                    s << "echo.\r\n";
                    s << "echo  Wipe complete. Shutting down in 5 seconds...\r\n";
                    s << "ping 127.0.0.1 -n 6 > nul\r\n";
                    s << "wpeutil shutdown\r\n";
                }
            }
            break;

        case WinREPatcher::WipeStrategy::QuickFormatAndTrim:
            {
                {
                    string p = mountDir + "\\Windows\\System32\\dp_ssd_prep.txt";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "select disk " << diskNumber << "\r\n";
                    s << "clean\r\n";
                    s << "create partition primary\r\n";
                    s << "assign letter=Z\r\n";
                    s << "exit\r\n";
                }
                {
                    string p = mountDir + "\\Windows\\System32\\dp_ssd_cleanup.txt";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "select disk " << diskNumber << "\r\n";
                    s << "clean\r\n";
                    s << "exit\r\n";
                }
                {
                    string p = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
                    ofstream s(p, ios::trunc);
                    if (!s.is_open()) return false;
                    s << "@echo off\r\n";
                    s << "echo.\r\n";
                    s << "echo  ====================================================\r\n";
                    s << "echo   WinRE Full Disk Wipe (SSD) ^| Disk " << diskNumber << "\r\n";
                    s << "echo   ALL DATA will be permanently erased via TRIM!\r\n";
                    s << "echo  ====================================================\r\n";
                    s << "echo.\r\n";
                    s << "echo  [1/3] Preparing SSD (removing partitions ^& assigning Z:)...\r\n";
                    s << "diskpart /s " << kWinRESys32 << "dp_ssd_prep.txt\r\n";
                    s << "echo.\r\n";
                    s << "echo  [2/3] Quick format + Explicit TRIM (wipes NAND cells)...\r\n";
                    s << "format Z: /FS:NTFS /Q /Y\r\n";
                    s << "defrag Z: /L\r\n";
                    s << "echo.\r\n";
                    s << "echo  [3/3] Final cleanup (returning disk to RAW state)...\r\n";
                    s << "diskpart /s " << kWinRESys32 << "dp_ssd_cleanup.txt\r\n";
                    s << "echo.\r\n";
                    s << "echo  Wipe complete. Shutting down in 5 seconds...\r\n";
                    s << "ping 127.0.0.1 -n 6 > nul\r\n";
                    s << "wpeutil shutdown\r\n";
                }
            }
            break;
        }

        return true;
    }

    bool PatchWinpeshl(const string& mountDir)
    {
        string iniPath = mountDir + "\\Windows\\System32\\winpeshl.ini";

        // Show current content
        string original;
        {
            ifstream in(iniPath);
            if (in.is_open())
            {
                ostringstream oss;
                oss << in.rdbuf();
                original = oss.str();
            }
        }
        cout << "  " << YELLOW << "--- winpeshl.ini (before) ---" << RESET << "\n"
                  << (original.empty() ? "  (empty)\n" : original) << "\n";

        ofstream out(iniPath, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  " << BRED << "[ERROR]" << RESET
                 << " Cannot open winpeshl.ini for writing.\n";
            return false;
        }

        out << "[LaunchApps]\r\n";
        out << "X:\\Windows\\System32\\cmd.exe, /c X:\\Windows\\System32\\wipe_launch.cmd\r\n";

        cout << "  " << YELLOW << "--- winpeshl.ini (after) ---" << RESET << "\n"
                  << "  [LaunchApps]\n"
                  << "  X:\\Windows\\System32\\cmd.exe, "
                     "/c X:\\Windows\\System32\\wipe_launch.cmd\n\n";
        return true;
    }
}
