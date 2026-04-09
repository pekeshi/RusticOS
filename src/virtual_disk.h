#ifndef VIRTUAL_DISK_H
#define VIRTUAL_DISK_H

#include "types.h"

// Extremely simple in-memory virtual disk to represent sectors in the OS image
// This is not persistent across runs unless the bootloader/loader copies sectors
// into/out of this buffer. It provides a clean block interface for the filesystem.

static const uint32_t VDISK_SECTOR_SIZE = 512;
static const uint32_t VDISK_NUM_SECTORS = 4096; // 2 MiB image

class VirtualDisk {
public:
    VirtualDisk();
    void clear();
    bool read_sector(uint32_t lba, void* out_buffer);     // returns false if out of range
    bool write_sector(uint32_t lba, const void* in_buffer); // returns false if out of range
};

extern VirtualDisk vdisk;

#endif // VIRTUAL_DISK_H