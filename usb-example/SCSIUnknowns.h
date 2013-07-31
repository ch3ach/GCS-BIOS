/* 
 * File:   messagehistory.h
 * Author: gandhi
 *
 * Created on 6. Februar 2013, 23:04
 */

#ifndef SCSIUNKNOWNS_H
#define	SCSIUNKNOWNS_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

    int32_t bin2hex(char* buf, const uint8_t data);
    void SCSIUnknowns_init(void* buffer, int32_t size);
    void SCSIUnknowns_logPackage(uint8_t SCSICommand, uint8_t LUN, uint8_t CBLength, void* CBData);
    int32_t SCSIUnknowns_nextASCII(void* out, int32_t outsize);
    void SCSIUnknowns_restartASCII(void);
    void SCSIUnknowns_disposeData(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SCSIUNKNOWNS_H */

