#include "ramdisk.h"

#define RAMDISK_SIZE            (128*1024)
#define RAMDISK_BLOCKSIZE       512 //256 is the minimum; it will not work if this value is any smaller

static uint8_t* ramdisk_data = ((uint8_t*) 0x20000000);

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

uint32_t ramdisk_read(uint32_t addr, const void* buffer, uint32_t len) {
    uint32_t n;
    uint32_t* buf = (uint32_t*) buffer;
    uint32_t* source = (uint32_t*) & ramdisk_data[addr];

    len /= sizeof (uint32_t);

    for (n = 0; n < len; n++) {
        buf[n] = source[n];
    }

    return n *= sizeof (uint32_t);
}

uint32_t ramdisk_write(uint32_t addr, const void* buffer, uint32_t len) {
    uint32_t n;
    uint32_t* buf = (uint32_t*) buffer;
    uint32_t* dest = (uint32_t*) & ramdisk_data[addr];

    len /= sizeof (uint32_t);

    for (n = 0; n < len; n++) {
        dest[n] = buf[n];
    }

    return n *= sizeof (uint32_t);
}

