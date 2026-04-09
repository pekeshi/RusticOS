#include "types.h"
#include "virtual_disk.h"

static uint8_t VDISK_BUFFER[VDISK_SECTOR_SIZE * VDISK_NUM_SECTORS];

VirtualDisk vdisk;

VirtualDisk::VirtualDisk() {
    // Optionally zero on startup
}

void VirtualDisk::clear() {
    for (uint32_t i = 0; i < VDISK_SECTOR_SIZE * VDISK_NUM_SECTORS; ++i) {
        VDISK_BUFFER[i] = 0;
    }
}

bool VirtualDisk::read_sector(uint32_t lba, void* out_buffer) {
    if (!out_buffer || lba >= VDISK_NUM_SECTORS) return false;
    uint8_t* dst = reinterpret_cast<uint8_t*>(out_buffer);
    const uint8_t* src = &VDISK_BUFFER[lba * VDISK_SECTOR_SIZE];
    for (uint32_t i = 0; i < VDISK_SECTOR_SIZE; ++i) dst[i] = src[i];
    return true;
}

bool VirtualDisk::write_sector(uint32_t lba, const void* in_buffer) {
    if (!in_buffer || lba >= VDISK_NUM_SECTORS) return false;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(in_buffer);
    uint8_t* dst = &VDISK_BUFFER[lba * VDISK_SECTOR_SIZE];
    for (uint32_t i = 0; i < VDISK_SECTOR_SIZE; ++i) dst[i] = src[i];
    return true;
}