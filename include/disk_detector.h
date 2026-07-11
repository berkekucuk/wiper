#pragma once

/**
 * @brief Detects the storage technology of a physical disk using Windows IOCTL.
 *
 * Two queries are made against the disk handle:
 *   1. StorageDeviceProperty   → STORAGE_DEVICE_DESCRIPTOR.BusType
 *      If BusType == BusTypeNvme → SSD_NVME.
 *   2. StorageDeviceSeekPenaltyProperty → DEVICE_SEEK_PENALTY_DESCRIPTOR
 *      IncursSeekPenalty == false → SSD_SATA
 *      IncursSeekPenalty == true  → HDD
 */
namespace DiskDetector
{
    enum class DiskType
    {
        HDD,        ///< Spinning hard disk drive
        SSD_SATA,   ///< Solid-state drive on SATA bus
        SSD_NVME,   ///< Solid-state drive on NVMe / PCIe bus
        Unknown     ///< Detection failed; caller should default to HDD strategy
    };

    /**
     * @brief Detects the disk type for the given physical disk number.
     *
     * @param diskNumber Physical disk number (e.g. 0 for \\.\PhysicalDrive0).
     * @return DiskType enum value.
     */
    DiskType Detect(int diskNumber);

    /**
     * @brief Returns a human-readable string for a DiskType value.
     */
    const char* ToString(DiskType type);
}
