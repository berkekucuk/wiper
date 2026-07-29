#include "config.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace Config
{
    static std::string Trim(const std::string& s)
    {
        auto left = std::find_if_not(s.begin(), s.end(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        auto right = std::find_if_not(s.rbegin(), s.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; }).base();
        return (left < right) ? std::string(left, right) : "";
    }

    static std::string ExeDir()
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path = exePath;
        auto pos = path.find_last_of("\\/");
        if (pos != std::string::npos)
            path.resize(pos);
        return path;
    }

    static bool ParseConfig(std::ifstream& in, WipeConfig& config)
    {
        std::string line;
        while (std::getline(in, line))
        {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;

            auto eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = Trim(line.substr(0, eq));
            std::string val = Trim(line.substr(eq + 1));
            auto comment = val.find('#');
            if (comment != std::string::npos)
                val = Trim(val.substr(0, comment));

            for (auto& c : key) c = static_cast<char>(std::tolower(c));

            if (key == "mode")
            {
                for (auto& c : val) c = static_cast<char>(std::tolower(c));
                config.mode = val;
            }
            else if (key == "disk_number")
            {
                config.diskNumber = std::stoi(val);
            }
            else if (key == "hdd_diskwipe_strategy")
            {
                for (auto& c : val) c = static_cast<char>(std::tolower(c));
                config.hddDiskwipeStrategy = val;
            }
            else if (key == "ssd_diskwipe_strategy")
            {
                for (auto& c : val) c = static_cast<char>(std::tolower(c));
                config.ssdDiskwipeStrategy = val;
            }
            else if (key == "wipe_list")
            {
                config.wipeList = val;
            }
            else if (key == "hdd_filewipe_strategy")
            {
                for (auto& c : val) c = static_cast<char>(std::tolower(c));
                config.hddFileStrategy = val;
            }
            else if (key == "ssd_filewipe_strategy")
            {
                for (auto& c : val) c = static_cast<char>(std::tolower(c));
                config.ssdFileStrategy = val;
            }
        }

        if (config.mode.empty())
        {
            std::cerr << "  [ERROR] 'mode' not set in config file.\n";
            return false;
        }

        return true;
    }

    bool LoadConfig(WipeConfig& config, const std::string& path)
    {
        // If caller provides an absolute/relative path with separator, use it as-is.
        if (path.find_first_of("\\/") != std::string::npos)
        {
            std::ifstream in(path);
            if (!in.is_open())
            {
                std::cerr << "  [ERROR] Could not open config file: " << path << "\n";
                return false;
            }
            return ParseConfig(in, config);
        }

        // Try exe directory first.
        std::string resolved = ExeDir() + "\\" + path;
        {
            std::ifstream in(resolved);
            if (in.is_open())
                return ParseConfig(in, config);
        }

        // Fallback: one level up from exe dir (project root during development).
        auto pos = ExeDir().find_last_of("\\/");
        if (pos != std::string::npos)
            resolved = ExeDir().substr(0, pos) + "\\" + path;

        std::ifstream in(resolved);
        if (!in.is_open())
        {
            std::cerr << "  [ERROR] Could not open config file: " << resolved << "\n";
            return false;
        }

        return ParseConfig(in, config);
    }
}
