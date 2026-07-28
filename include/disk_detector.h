#pragma once

/** @brief Detects the storage technology of a physical disk via Windows IOCTL queries. */
namespace DiskDetector
{
    enum class DiskType
    {
        HDD,        ///< Spinning hard disk drive
        SSD_SATA,   ///< Solid-state drive on SATA bus
        SSD_NVME,   ///< Solid-state drive on NVMe / PCIe bus
        Unknown     ///< Detection failed; caller should default to HDD strategy
    };

    /** @brief Detects disk type for the given physical disk number. */
    DiskType Detect(int diskNumber);

    /** @brief Returns a human-readable string for a DiskType value. */
    const char* ToString(DiskType type);
}
