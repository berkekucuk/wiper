#include "winre_script_generator.h"
#include "disk_detector.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

namespace WinREScriptGenerator
{
    bool GenerateFullDiskWipeScripts(const string& mountDir, int diskNumber, bool isSSD)
    {

        if (!isSSD)
        {
            // HDD: clean all -> shutdown
            {
                string p = mountDir + "\\Windows\\System32\\clean_disk_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open())
                {
                    cerr << "  [ERROR] Cannot create clean_disk_script.txt\n";
                    return false;
                }
                s << "select disk " << diskNumber << "\r\n";
                s << "clean all\r\n";
                s << "exit\r\n";
            }

            {
                string p = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
                ofstream s(p, ios::trunc);
                if (!s.is_open())
                {
                    cerr << "  [ERROR] Cannot create wipe_launch.cmd\n";
                    return false;
                }
                s << "@echo off\r\n";
                s << "echo.\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo   WinRE Full Disk Wipe (HDD) ^| Disk " << diskNumber << "\r\n";
                s << "echo   ALL DATA will be permanently erased!\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo.\r\n";
                s << "echo  [1/2] Running diskpart clean all...\r\n";
                s << "echo  This may take 30-60 minutes for large drives.\r\n";
                s << "echo.\r\n";
                s << "diskpart /s X:\\Windows\\System32\\clean_disk_script.txt\r\n";
                s << "echo.\r\n";
                s << "echo  [2/2] Disk wiped. Shutting down in 5 seconds...\r\n";
                s << "ping 127.0.0.1 -n 6 > nul\r\n";
                s << "wpeutil shutdown\r\n";
            }
        }

        else
        {
            // SSD: clean -> create temp partition -> format /Q -> defrag /L -> clean
            {
                string p = mountDir + "\\Windows\\System32\\prepare_ssd_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open())
                {
                    cerr << "  [ERROR] Cannot create prepare_ssd_script.txt\n";
                    return false;
                }
                s << "select disk " << diskNumber << "\r\n";
                s << "clean\r\n";
                s << "create partition primary\r\n";
                s << "assign letter=Z\r\n";
                s << "exit\r\n";
            }

            {
                string p = mountDir + "\\Windows\\System32\\final_clean_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open())
                {
                    cerr << "  [ERROR] Cannot create final_clean_script.txt\n";
                    return false;
                }
                s << "select disk " << diskNumber << "\r\n";
                s << "clean\r\n";
                s << "exit\r\n";
            }

            {
                string p = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
                ofstream s(p, ios::trunc);
                if (!s.is_open())
                {
                    cerr << "  [ERROR] Cannot create wipe_launch.cmd\n";
                    return false;
                }
                s << "@echo off\r\n";
                s << "echo.\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo   WinRE Full Disk Wipe (SSD) ^| Disk " << diskNumber << "\r\n";
                s << "echo   ALL DATA will be permanently erased via TRIM!\r\n";
                s << "echo  ====================================================\r\n";
                s << "echo.\r\n";
                s << "echo  [1/3] Preparing SSD (removing partitions ^& assigning Z:)...\r\n";
                s << "diskpart /s X:\\Windows\\System32\\prepare_ssd_script.txt\r\n";
                s << "echo.\r\n";
                s << "echo  [2/3] Quick format + Explicit TRIM (wipes NAND cells)...\r\n";
                s << "format Z: /FS:NTFS /Q /Y\r\n";
                s << "defrag Z: /L\r\n";
                s << "echo.\r\n";
                s << "echo  [3/3] Final cleanup (returning disk to RAW state)...\r\n";
                s << "diskpart /s X:\\Windows\\System32\\final_clean_script.txt\r\n";
                s << "echo.\r\n";
                s << "echo  Wipe complete. Shutting down in 5 seconds...\r\n";
                s << "ping 127.0.0.1 -n 6 > nul\r\n";
                s << "wpeutil shutdown\r\n";
            }
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
        cout << "  --- winpeshl.ini (before) ---\n"
                  << (original.empty() ? "  (empty)\n" : original) << "\n";

        ofstream out(iniPath, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  [ERROR] Cannot open winpeshl.ini for writing.\n";
            return false;
        }

        out << "[LaunchApps]\r\n";
        out << "X:\\Windows\\System32\\cmd.exe, /c X:\\Windows\\System32\\wipe_launch.cmd\r\n";

        cout << "  --- winpeshl.ini (after) ---\n"
                  << "  [LaunchApps]\n"
                  << "  X:\\Windows\\System32\\cmd.exe, "
                     "/c X:\\Windows\\System32\\wipe_launch.cmd\n\n";
        return true;
    }
}
