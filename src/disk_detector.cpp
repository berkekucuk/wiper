#include "disk_detector.h"
#include <windows.h>
#include <winioctl.h>

#include <iostream>
#include <string>

using namespace std;

namespace DiskDetector
{
    DiskType Detect(int diskNumber)
    {
        // Open the physical drive for property queries (no data access needed)
        string path = "\\\\.\\PhysicalDrive" + to_string(diskNumber);

        HANDLE h = CreateFileA(
            path.c_str(),
            0,                                      // no read/write — metadata only
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (h == INVALID_HANDLE_VALUE)
        {
            cerr << "  [WARN] Cannot open " << path
                      << " (code " << GetLastError()
                      << "). Defaulting to HDD strategy.\n";
            return DiskType::Unknown;
        }

        DiskType result = DiskType::Unknown;
        DWORD    bytesReturned = 0;

        // ── Query 1: BusType ── detect NVMe ──────────────────────────────────
        {
            STORAGE_PROPERTY_QUERY q = {};
            q.PropertyId = StorageDeviceProperty;
            q.QueryType  = PropertyStandardQuery;

            BYTE buf[512] = {};
            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                                &q, sizeof(q),
                                buf, sizeof(buf),
                                &bytesReturned, nullptr))
            {
                auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf);
                if (desc->BusType == BusTypeNvme)
                    result = DiskType::SSD_NVME;
            }
        }

        // ── Query 2: SeekPenalty ── distinguish SSD-SATA from HDD ────────────
        if (result == DiskType::Unknown)
        {
            STORAGE_PROPERTY_QUERY q = {};
            q.PropertyId = StorageDeviceSeekPenaltyProperty;
            q.QueryType  = PropertyStandardQuery;

            DEVICE_SEEK_PENALTY_DESCRIPTOR d = {};
            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                                &q, sizeof(q),
                                &d, sizeof(d),
                                &bytesReturned, nullptr))
            {
                // No seek penalty → solid-state
                result = d.IncursSeekPenalty ? DiskType::HDD : DiskType::SSD_SATA;
            }
            else
            {
                cerr << "  [WARN] SeekPenalty query failed "
                          << "(code " << GetLastError()
                          << "). Defaulting to HDD strategy.\n";
            }
        }

        CloseHandle(h);
        return result;
    }

    const char* ToString(DiskType type)
    {
        switch (type)
        {
        case DiskType::HDD:      return "HDD (rotational)";
        case DiskType::SSD_SATA: return "SSD (SATA)";
        case DiskType::SSD_NVME: return "SSD (NVMe)";
        default:                 return "Unknown (defaulting to HDD strategy)";
        }
    }
}
