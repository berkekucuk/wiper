#include "winre_finder.h"
#include "cli_utils.h"
#include <windows.h>

#include <array>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace
{
    /**
     * @brief Executes a command-line command and returns its stdout as a string.
     *
     * @param cmd Command to execute.
     * @return stdout output of the command, or an empty string on failure.
     */
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

    /**
     * @brief Reads the WinRE path from the Windows Registry.
     *
     * @param outPath Receives the path string on success.
     * @return true  Read was successful.
     * @return false Registry key not found or could not be read.
     */
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

    /**
     * @brief Parses `reagentc /info` output to extract the WinRE device path.
     *
     * @param output    Raw output from reagentc.
     * @param outPath   Receives the extracted path.
     * @param outStatus Receives the extracted status.
     * @return true  A WinRE device path was found.
     * @return false No WinRE device path was found in the output.
     */
    bool ParseReagentcOutput(const std::string& output,
                             std::string&       outPath,
                             std::string&       outStatus)
    {
        constexpr const char* kWinREDevicePrefix = "\\\\?\\GLOBALROOT\\device\\";

        std::istringstream stream(output);
        std::string        line;

        while (getline(stream, line))
        {
            // Strip leading and trailing whitespace
            auto trim = [](const std::string& s) -> std::string {
                size_t start = s.find_first_not_of(" \t\r\n");
                size_t end   = s.find_last_not_of(" \t\r\n");
                return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };

            std::string trimmed = trim(line);

            // The GLOBALROOT device path is not localized, unlike reagentc labels.
            const size_t pathStart = trimmed.find(kWinREDevicePrefix);
            if (pathStart != std::string::npos)
            {
                outPath = trimmed.substr(pathStart);
                outStatus = "Detected";
                return true;
            }
        }

        return false;
    }
} // anonymous namespace

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
} // namespace WinREFinder
