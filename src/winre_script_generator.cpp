#include "winre_script_generator.h"
#include "disk_detector.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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
            << (isSSD ? "SSD (quick format + explicit TRIM)" : "HDD (format /P:1 random overwrite)")
            << "\r\n";
        out << "echo  ====================================================\r\n";
        out << "echo.\r\n";
        
        out << "echo  [1/3] Assigning temporary drive letter Z:...\r\n";
        out << "diskpart /s X:\\Windows\\System32\\assign_script.txt\r\n";
        out << "echo.\r\n";

        if (isSSD)
        {
            out << "echo  [2/3] Quick format + Explicit TRIM (SSD safe wipe)...\r\n";
            out << "echo.\r\n";
            out << "format Z: /FS:NTFS /Q /Y\r\n";
            out << "defrag Z: /L\r\n";  
            out << "echo.\r\n";
            out << "echo  [3/3] Removing temporary drive letter Z:...\r\n";
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
        if (!WriteAssignScript(mountDir, diskNumber, partitionNumber)) return false;
        if (!WriteRemoveScript(mountDir, diskNumber, partitionNumber)) return false;
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