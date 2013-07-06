#include "ramdisk.h"

#define RAMDISK_SIZE            (128*1024)
#define RAMDISK_BLOCKSIZE       256 //256 is the minimum; it will not work if this value is any smaller

static uint32_t* ramdisk_data = (uint32_t*) 0x20000000;

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

uint32_t ramdisk_read(uint32_t addr, uint32_t* buffer, uint32_t len) {
    uint32_t n;

    len /= sizeof (uint32_t);
    addr /= sizeof (uint32_t);

    for (n = 0; n < len; n++) {
        buffer[n] = ramdisk_data[n + addr];
    }

    return n *= sizeof (uint32_t);
}

