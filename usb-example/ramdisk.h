/* 
 * File:   ramdisk.h
 * Author: gandhi
 *
 * Created on 28. Februar 2013, 23:47
 */

#ifndef RAMDISK_H
#define	RAMDISK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef	__cplusplus
extern "C" {
#endif
    void ramdisk_init(void);
    bool ramdisk_ready(void);
    uint32_t ramdisk_getSize(void);
    uint32_t ramdisk_getBlockSize(void);
    uint32_t ramdisk_getBlockCount(void);

#ifdef	__cplusplus
}
#endif

#endif	/* RAMDISK_H */

