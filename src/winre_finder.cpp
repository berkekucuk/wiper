#include "winre_finder.h"
#include "cli_utils.h"
#include <windows.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /** @brief Executes a command and returns its stdout as a string. */
    std::string RunCommand(const std::string& cmd)
    {
        std::string result;
        std::array<char, 256> buffer{};

        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe)
            return result;

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
            result += buffer.data();

        _pclose(pipe);
        return result;
    }

    /** @brief Reads the WinRE path from the Windows Registry. */
    bool ReadFromRegistry(std::string& outPath)
    {
        // WinRE path may be stored under either of these registry locations.
        const char* keys[] = {
            "SOFTWARE\\Microsoft\\RecoveryEnvironment",
            "SYSTEM\\CurrentControlSet\\Control\\MiniNT"
        };
        const char* valueNames[] = {
            "WinRELocation",
            "WinREPath"
        };

        for (int i = 0; i < 2; ++i)
        {
            HKEY hKey = nullptr;
            LONG ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, keys[i], 0, KEY_READ, &hKey);
            if (ret != ERROR_SUCCESS)
                continue;

            char  buf[MAX_PATH] = {};
            DWORD bufSize = sizeof(buf);
            DWORD type    = 0;

            ret = RegQueryValueExA(hKey, valueNames[i], nullptr, &type,
                                   reinterpret_cast<LPBYTE>(buf), &bufSize);
            RegCloseKey(hKey);

            if (ret == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ))
            {
                outPath = buf;
                return true;
            }
        }
        return false;
    }

    /** @brief Parses reagentc /info output to extract the WinRE device path and status. */
    bool ParseReagentcOutput(const std::string& output,
                             std::string&       outPath,
                             std::string&       outStatus)
    {
        constexpr const char* kWinREDevicePrefix = "\\\\?\\GLOBALROOT\\device\\";
        bool pathFound = false;

        std::istringstream stream(output);
        std::string        line;

        while (getline(stream, line))
        {
            auto trim = [](const std::string& s) -> std::string {
                size_t start = s.find_first_not_of(" \t\r\n");
                size_t end   = s.find_last_not_of(" \t\r\n");
                return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };

            std::string trimmed = trim(line);

            const size_t pathStart = trimmed.find(kWinREDevicePrefix);
            if (pathStart != std::string::npos)
            {
                outPath = trimmed.substr(pathStart);
                pathFound = true;
                continue;
            }

            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos)
            {
                std::string afterColon = trim(trimmed.substr(colonPos + 1));
                if (!afterColon.empty() &&
                    afterColon.length() < 50 &&
                    afterColon.find('\\') == std::string::npos)
                {
                    std::string beforeColon = trim(trimmed.substr(0, colonPos));
                    std::string lowerLabel;
                    for (char c : beforeColon)
                        lowerLabel += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                    if (lowerLabel.find("status") != std::string::npos ||
                        lowerLabel.find("durum") != std::string::npos)
                    {
                        outStatus = afterColon;
                    }
                }
            }
        }

        return pathFound;
    }
}

// ---------------------------------------------------------------------------
// WinREFinder interface implementation
// ---------------------------------------------------------------------------
namespace WinREFinder
{
    bool FindLocation(std::string& outPath, std::string& outStatus)
    {
        outStatus = "Unknown";

        // 1. Try reagentc /info first (most reliable source)
        // Use the full system path to prevent PATH-based binary hijacking.
        std::string reagentOutput = RunCommand("C:\\Windows\\System32\\reagentc.exe /info 2>&1");
        if (!reagentOutput.empty())
        {
            if (ParseReagentcOutput(reagentOutput, outPath, outStatus))
                return true;
        }

        // 2. Fallback: read from Registry
        if (ReadFromRegistry(outPath))
            return true;

        return false;
    }

    void PrintInfo(const std::string& path, const std::string& status)
    {
        std::cout << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "+--------------------------------------------------+" << CLIUtils::Colors::RESET << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "|" << CLIUtils::Colors::RESET << "        Windows Recovery Environment (WinRE)      " << CLIUtils::Colors::CYAN << "|" << CLIUtils::Colors::RESET << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "+--------------------------------------------------+" << CLIUtils::Colors::RESET << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "|" << CLIUtils::Colors::RESET << " Status : " << status << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "|" << CLIUtils::Colors::RESET << " Path   : " << path   << "\n";
        std::cout << "  " << CLIUtils::Colors::CYAN << "+--------------------------------------------------+" << CLIUtils::Colors::RESET << "\n";
        std::cout << "\n";
    }

    bool EnsureEnabled()
    {
        std::string path, status;
        FindLocation(path, status);

        if (status == "Unknown")
        {
            std::cout << "  Could not determine WinRE status, skipping enable check.\n";
            return true;
        }

        auto toLower = [](const std::string& s) -> std::string {
            std::string result;
            for (char c : s)
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        };

        std::string lowerStatus = toLower(status);

        if (lowerStatus == "disabled" || lowerStatus == "devre dışı" || lowerStatus == "devre disi")
        {
            std::cout << "  WinRE is disabled. Enabling...\n";
            std::string result = RunCommand("C:\\Windows\\System32\\reagentc.exe /enable 2>&1");

            if (result.find("Operation Successful") != std::string::npos)
            {
                std::cout << "  WinRE enabled successfully.\n";
                return true;
            }
            else
            {
                std::cout << "  Failed to enable WinRE.\n";
                return false;
            }
        }

        std::cout << "  WinRE is already enabled.\n";
        return true;
    }
} 
