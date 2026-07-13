#include "winre_script_generator.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

namespace
{
    bool WriteAssignScript(const string& mountDir,
                           int diskNumber, int partitionNumber)
    {
        string path = mountDir + "\\Windows\\System32\\assign_script.txt";
        ofstream out(path, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  [ERROR] Cannot create assign_script.txt\n";
            return false;
        }

        out << "select disk "      << diskNumber      << "\r\n";
        out << "select partition " << partitionNumber << "\r\n";
        out << "assign letter=Z\r\n";
        out << "exit\r\n";

        cout << "  assign_script.txt written:\n"
                  << "    select disk "      << diskNumber      << "\n"
                  << "    select partition " << partitionNumber << "\n"
                  << "    assign letter=Z\n"
                  << "    exit\n\n";
        return true;
    }

    bool WriteRemoveScript(const string& mountDir,
                           int diskNumber, int partitionNumber)
    {
        string path = mountDir + "\\Windows\\System32\\remove_script.txt";
        ofstream out(path, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  [ERROR] Cannot create remove_script.txt\n";
            return false;
        }

        out << "select disk "      << diskNumber      << "\r\n";
        out << "select partition " << partitionNumber << "\r\n";
        out << "remove letter=Z\r\n";
        out << "exit\r\n";

        cout << "  remove_script.txt written:\n"
                  << "    select disk "      << diskNumber      << "\n"
                  << "    select partition " << partitionNumber << "\n"
                  << "    remove letter=Z\n"
                  << "    exit\n\n";
        return true;
    }

    bool WriteRetrimScript(const string& mountDir)
    {
        string path = mountDir + "\\Windows\\System32\\retrim_script.txt";
        ofstream out(path, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  [ERROR] Cannot create retrim_script.txt\n";
            return false;
        }

        out << "select volume Z\r\n";
        out << "retrim\r\n";
        out << "exit\r\n";

        cout << "  retrim_script.txt written (select volume Z → retrim → exit)\n\n";
        return true;
    }

    bool WritePartitionLaunchCmd(const string&     mountDir,
                                 int                    diskNumber,
                                 int                    partitionNumber,
                                 DiskDetector::DiskType diskType)
    {
        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
             diskType == DiskDetector::DiskType::SSD_NVME);

        string path = mountDir + "\\Windows\\System32\\wipe_launch.cmd";
        ofstream out(path, ios::trunc);
        if (!out.is_open())
        {
            cerr << "  [ERROR] Cannot create wipe_launch.cmd\n";
            return false;
        }

        out << "@echo off\r\n";
        out << "echo.\r\n";
        out << "echo  ====================================================\r\n";
        out << "echo   WinRE Secure Wipe  ^|  Disk " << diskNumber
            << "  Partition "   << partitionNumber    << "\r\n";
        out << "echo   Strategy : "
            << (isSSD ? "SSD (quick format + TRIM)" : "HDD (format /P:1 random)")
            << "\r\n";
        out << "echo  ====================================================\r\n";
        out << "echo.\r\n";
        out << "echo  [1/3] Assigning temporary drive letter Z:...\r\n";
        out << "diskpart /s X:\\Windows\\System32\\assign_script.txt\r\n";
        out << "echo.\r\n";

        if (isSSD)
        {
            out << "echo  [2/3] Quick format (SSD — no unnecessary write cycles)...\r\n";
            out << "echo.\r\n";
            out << "format Z: /FS:NTFS /Q /Y\r\n";
            out << "echo.\r\n";
            out << "echo  [3/3] Sending TRIM to SSD controller + removing drive letter...\r\n";
            out << "diskpart /s X:\\Windows\\System32\\retrim_script.txt\r\n";
        }
        else
        {
            out << "echo  [2/3] Overwriting with random data (format /P:1)...\r\n";
            out << "echo  This may take 15-30 minutes for large drives.\r\n";
            out << "echo.\r\n";
            out << "format Z: /FS:NTFS /P:1 /Y\r\n";
            out << "echo.\r\n";
            out << "echo  [3/3] Removing temporary drive letter Z:...\r\n";
        }
        out << "diskpart /s X:\\Windows\\System32\\remove_script.txt\r\n";
        out << "echo.\r\n";

        out << "echo  Wipe complete. Rebooting in 5 seconds...\r\n";
        out << "ping 127.0.0.1 -n 6 > nul\r\n";
        out << "wpeutil reboot\r\n";

        cout << "  wipe_launch.cmd written\n"
                  << "    Strategy  : "
                  << (isSSD ? "SSD (format /Q + TRIM)" : "HDD (format /P:1)")
                  << "\n"
                  << "    Phase 1   : diskpart assign Z:\n"
                  << "    Phase 2   : " << (isSSD ? "format /Q /Y" : "format /P:1 /Y") << "\n"
                  << "    Phase 3   : " << (isSSD ? "retrim + " : "") << "remove Z: → reboot\n\n";
        return true;
    }
}

namespace WinREScriptGenerator
{
    bool GeneratePartitionWipeScripts(const string& mountDir,
                                      int diskNumber,
                                      int partitionNumber,
                                      DiskDetector::DiskType diskType)
    {
        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
             diskType == DiskDetector::DiskType::SSD_NVME);

        if (!WriteAssignScript(mountDir, diskNumber, partitionNumber)) return false;
        if (!WriteRemoveScript(mountDir, diskNumber, partitionNumber)) return false;
        if (isSSD && !WriteRetrimScript(mountDir)) return false;
        if (!WritePartitionLaunchCmd(mountDir, diskNumber, partitionNumber, diskType)) return false;

        return true;
    }

    bool GenerateFullDiskWipeScripts(const string& mountDir,
                                     int diskNumber,
                                     DiskDetector::DiskType diskType)
    {
        const bool isSSD =
            (diskType == DiskDetector::DiskType::SSD_SATA ||
             diskType == DiskDetector::DiskType::SSD_NVME);

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
            // SSD: clean -> create temp partition -> format /Q -> retrim -> clean
            {
                string p = mountDir + "\\Windows\\System32\\prepare_ssd_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open()) return false;
                s << "select disk " << diskNumber << "\r\n";
                s << "clean\r\n";
                s << "create partition primary\r\n";
                s << "assign letter=Z\r\n";
                s << "exit\r\n";
            }
            {
                string p = mountDir + "\\Windows\\System32\\retrim_ssd_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open()) return false;
                s << "select volume Z\r\n";
                s << "retrim\r\n";
                s << "exit\r\n";
            }
            {
                string p = mountDir + "\\Windows\\System32\\final_clean_script.txt";
                ofstream s(p, ios::trunc);
                if (!s.is_open()) return false;
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
                s << "echo  [2/3] Sending TRIM to entire SSD capacity...\r\n";
                s << "format Z: /FS:NTFS /Q /Y\r\n";
                s << "diskpart /s X:\\Windows\\System32\\retrim_ssd_script.txt\r\n";
                s << "echo.\r\n";
                s << "echo  [3/3] Final cleanup. Shutting down in 5 seconds...\r\n";
                s << "diskpart /s X:\\Windows\\System32\\final_clean_script.txt\r\n";
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
