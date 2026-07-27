#include "file_wipe_runner.h"
#include "cli_utils.h"
#include "disk_detector.h"

#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    std::wstring ToWide(const std::string& utf8)
    {
        if (utf8.empty()) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        if (len <= 0) return {};
        std::wstring wstr(static_cast<size_t>(len) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wstr.data(), len);
        return wstr;
    }

    std::string WideToUTF8(const std::wstring& wide)
    {
        if (wide.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(static_cast<size_t>(len) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), len, nullptr, nullptr);
        return result;
    }

    void FillRandom(std::vector<char>& buf)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : buf)
            b = static_cast<char>(dist(gen));
    }

    // -----------------------------------------------------------------------
    // Resolve the physical disk number for a given file path
    // -----------------------------------------------------------------------
    int GetDiskNumberFromPath(const std::string& path)
    {
        std::wstring wPath = ToWide(path);
        wchar_t volumeRoot[MAX_PATH] = {};
        if (!GetVolumePathNameW(wPath.c_str(), volumeRoot, MAX_PATH))
            return -1;

        std::wstring volPath = L"\\\\.\\";
        volPath += volumeRoot[0];
        volPath += L":";

        HANDLE hVol = CreateFileW(volPath.c_str(), 0,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol == INVALID_HANDLE_VALUE)
            return -1;

        VOLUME_DISK_EXTENTS extents = {};
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                  nullptr, 0, &extents, sizeof(extents),
                                  &bytesReturned, nullptr);
        CloseHandle(hVol);

        if (!ok || extents.NumberOfDiskExtents == 0)
            return -1;

        return extents.Extents[0].DiskNumber;
    }

    // -----------------------------------------------------------------------
    // Returns true if the file resides on an SSD
    // -----------------------------------------------------------------------
    bool IsFileOnSSD(const std::string& path)
    {
        int diskNumber = GetDiskNumberFromPath(path);
        if (diskNumber < 0)
        {
            std::cout << "  " << CLIUtils::Colors::YELLOW << "[WARN]" << CLIUtils::Colors::RESET
                      << " Cannot detect disk type for file, assuming HDD.\n";
            return false;
        }

        DiskDetector::DiskType dt = DiskDetector::Detect(diskNumber);
        return (dt == DiskDetector::DiskType::SSD_SATA ||
                dt == DiskDetector::DiskType::SSD_NVME);
    }

    bool WriteBuffer(HANDLE hFile, const std::vector<char>& buf, DWORD bufSize, LONGLONG fileSize)
    {
        SetFilePointer(hFile, 0, nullptr, FILE_BEGIN);
        LONGLONG remaining = fileSize;
        while (remaining > 0)
        {
            DWORD chunk = (remaining > bufSize) ? bufSize : static_cast<DWORD>(remaining);
            DWORD written = 0;
            if (!WriteFile(hFile, buf.data(), chunk, &written, nullptr) || written != chunk)
                return false;
            remaining -= written;
        }
        return true;
    }

    bool WipeFileContent(const std::string& filePath, FileWiper::FileWipeStrategy strategy)
    {
        std::wstring wPath = ToWide(filePath);
        HANDLE hFile = CreateFileW(wPath.c_str(),
                                   GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr,
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                      << " Cannot open file for writing: " << filePath
                      << " (error " << err << ")\n";
            return false;
        }

        LARGE_INTEGER liSize;
        if (!GetFileSizeEx(hFile, &liSize))
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                      << " Cannot determine file size: " << filePath << "\n";
            CloseHandle(hFile);
            return false;
        }

        if (liSize.QuadPart == 0)
        {
            CloseHandle(hFile);
            return true;
        }

        constexpr DWORD kBufSize = 65536;
        std::vector<char> buf(kBufSize);
        bool ok = true;

        auto doPass = [&](char pattern) {
            std::fill(buf.begin(), buf.end(), pattern);
            if (!WriteBuffer(hFile, buf, kBufSize, liSize.QuadPart))
            {
                ok = false;
                return false;
            }
            FlushFileBuffers(hFile);
            return true;
        };

        auto doRandomPass = [&]() {
            FillRandom(buf);
            if (!WriteBuffer(hFile, buf, kBufSize, liSize.QuadPart))
            {
                ok = false;
                return false;
            }
            FlushFileBuffers(hFile);
            return true;
        };

        switch (strategy)
        {
        case FileWiper::FileWipeStrategy::ZeroFill:
            doPass(0);
            break;
        case FileWiper::FileWipeStrategy::RandomFill:
            doRandomPass();
            break;
        case FileWiper::FileWipeStrategy::DOD3:
            doRandomPass();
            if (!ok) break;
            for (auto& b : buf) b = ~b;
            if (!WriteBuffer(hFile, buf, kBufSize, liSize.QuadPart))
                ok = false;
            else
                FlushFileBuffers(hFile);
            if (!ok) break;
            doRandomPass();
            break;
        }

        CloseHandle(hFile);

        if (!ok)
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                      << " Write failed: " << filePath << "\n";

        return ok;
    }
} 

// ---------------------------------------------------------------------------
// FileWiper interface implementation
// ---------------------------------------------------------------------------
namespace FileWiper
{
    bool ParseFileList(const std::string& listPath, std::vector<std::string>& files)
    {
        std::ifstream in(listPath);
        if (!in.is_open())
        {
            std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                      << " Cannot open file: " << listPath << "\n";
            return false;
        }

        files.clear();
        std::string line;
        const auto trimPred = [](unsigned char c) { return std::isspace(c) != 0; };
        while (std::getline(in, line))
        {
            auto left  = std::find_if_not(line.begin(), line.end(), trimPred);
            auto right = std::find_if_not(line.rbegin(), line.rend(), trimPred).base();
            if (left < right)
            {
                std::string path(left, right);
                if (!path.empty())
                    files.push_back(std::move(path));
            }
        }
        return true;
    }

    bool WipeSingleFile(const std::string& path, FileWiper::FileWipeStrategy strategy)
    {
        std::error_code ec;
        std::wstring wPath = ToWide(path);

        DWORD attrs = GetFileAttributesW(wPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
            SetFileAttributesW(wPath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);

        bool onSSD = IsFileOnSSD(path);

        if (onSSD)
        {
            std::cout << "  SSD     : " << path << " (overwrite skipped, using TRIM)\n";
        }
        else
        {
            std::cout << "  Wiping  : " << path << "\n";
            if (!WipeFileContent(path, strategy))
                return false;
        }

        std::filesystem::path nativePath = std::filesystem::path(wPath);
        std::string parentDir = WideToUTF8(nativePath.parent_path().wstring());
        std::string tmpName;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            tmpName = parentDir + "\\" + std::to_string(rand()) + ".tmp";
            if (!std::filesystem::exists(tmpName, ec))
                break;
            tmpName.clear();
        }

        if (!tmpName.empty())
        {
            std::filesystem::rename(nativePath, tmpName, ec);
            if (!ec)
            {
                std::filesystem::remove(tmpName, ec);
                if (ec)
                {
                    std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                              << " Failed to delete: " << tmpName << " (" << ec.message() << ")\n";
                    return false;
                }
            }
            else
            {
                std::filesystem::remove(nativePath, ec);
                if (ec)
                {
                    std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                              << " Failed to delete: " << path << " (" << ec.message() << ")\n";
                    return false;
                }
            }
        }
        else
        {
            std::filesystem::remove(nativePath, ec);
            if (ec)
            {
                std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                          << " Failed to delete: " << path << " (" << ec.message() << ")\n";
                return false;
            }
        }

        std::cout << "  " << CLIUtils::Colors::BGREEN << "OK" << CLIUtils::Colors::RESET
                  << "      : " << path << "\n";
        return true;
    }

    bool WipeDirectory(const std::string& dirPath, FileWiper::FileWipeStrategy strategy)
    {
        std::error_code ec;

        std::cout << "  Entering: " << dirPath << "\n";

        for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec))
        {
            if (ec) break;
            if (entry.is_regular_file())
                WipeSingleFile(WideToUTF8(entry.path().wstring()), strategy);
        }

        std::filesystem::path nativeDirPath = std::filesystem::path(ToWide(dirPath));
        std::string parentDir = WideToUTF8(nativeDirPath.parent_path().wstring());
        std::string tmpDirName;
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            tmpDirName = parentDir + "\\" + std::to_string(rand()) + ".tmp";
            if (!std::filesystem::exists(tmpDirName, ec))
                break;
            tmpDirName.clear();
        }

        bool removed = false;
        if (!tmpDirName.empty())
        {
            std::filesystem::rename(nativeDirPath, tmpDirName, ec);
            if (!ec)
            {
                std::filesystem::remove_all(tmpDirName, ec);
                removed = !ec;
            }
            else
            {
                std::filesystem::remove_all(nativeDirPath, ec);
                removed = !ec;
            }
        }
        else
        {
            std::filesystem::remove_all(nativeDirPath, ec);
            removed = !ec;
        }

        if (removed)
        {
            std::cout << "  " << CLIUtils::Colors::BGREEN << "OK" << CLIUtils::Colors::RESET
                      << "      : " << dirPath << " (directory)\n";
            return true;
        }

        std::cerr << "  " << CLIUtils::Colors::BRED << "[ERROR]" << CLIUtils::Colors::RESET
                  << " Failed to remove directory: " << dirPath
                  << " (" << ec.message() << ")\n";
        return false;
    }

    bool WipeAndDelete(const std::string& path, FileWiper::FileWipeStrategy strategy)
    {
        // Use Windows API directly to check existence (more reliable than
        // std::filesystem::exists on MinGW for certain paths).
        DWORD attrs = GetFileAttributesW(ToWide(path).c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            DWORD err = GetLastError();
            std::cerr << "  " << CLIUtils::Colors::YELLOW << "[WARN]" << CLIUtils::Colors::RESET
                      << " Cannot access: " << path << " (error " << err << ")\n";
            return false;
        }

        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
            return WipeDirectory(path, strategy);

        return WipeSingleFile(path, strategy);
    }

    int WipeFiles(const std::string& listPath, FileWiper::FileWipeStrategy strategy)
    {
        std::vector<std::string> files;
        if (!ParseFileList(listPath, files))
            return 0;

        if (files.empty())
        {
            std::cout << "  " << CLIUtils::Colors::YELLOW << "[WARN]" << CLIUtils::Colors::RESET
                      << " No entries found in list file.\n";
            return 0;
        }

        std::cout << "  Entries to wipe : " << files.size() << "\n\n";

        int successCount = 0;
        for (const auto& f : files)
        {
            if (WipeAndDelete(f, strategy))
                ++successCount;
        }
        return successCount;
    }
    FileWiper::FileWipeStrategy StrategyFromString(const std::string& s)
    {
        if (s == "randomfill") return FileWiper::FileWipeStrategy::RandomFill;
        if (s == "dod3") return FileWiper::FileWipeStrategy::DOD3;
        return FileWiper::FileWipeStrategy::ZeroFill;
    }

    int RunFileWipe(const std::string& wipeList, FileWiper::FileWipeStrategy strategy)
    {
        CLIUtils::PrintFileWipeBanner(strategy);

        // Resolve relative paths against exe directory (not CWD)
        std::string resolved = wipeList;
        if (resolved.find_first_of("\\/") == std::string::npos)
        {
            char exePath[MAX_PATH] = {};
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string exeDir = exePath;
            auto pos = exeDir.find_last_of("\\/");
            if (pos != std::string::npos)
                exeDir.resize(pos);
            resolved = exeDir + "\\" + wipeList;
        }

        std::cout << "  List file : " << resolved << "\n\n";

        int wiped = WipeFiles(resolved, strategy);

        std::cout << "\n  " << CLIUtils::Colors::BCYAN << "--- Summary ---" << CLIUtils::Colors::RESET << "\n"
                  << "  Successfully wiped : " << wiped << "\n\n";

        std::cout << "  Press any key to exit...\n";
        std::cin.get();
        return (wiped > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
} 
