#include "ramdisk.h"

#define RAMDISK_SIZE            (128*1024)
#define RAMDISK_BLOCKSIZE       32

static uint8_t* ramdisk_data = (uint8_t*) 0x20000000;

void ramdisk_init(void) { }

bool ramdisk_ready(void) {
    return true;
}

uint32_t ramdisk_getSize(void) {
    return RAMDISK_SIZE;
}

uint32_t ramdisk_getBlockSize(void) {
    return RAMDISK_BLOCKSIZE;
}

uint32_t ramdisk_getBlockCount(void) {
    return (RAMDISK_SIZE / RAMDISK_BLOCKSIZE);
}

