/* 
 * File:   messagehistory.h
 * Author: gandhi
 *
 * Created on 6. Februar 2013, 23:04
 */

#ifndef MESSAGEHISTORY_H
#define	MESSAGEHISTORY_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

void history_init(uint8_t* buffer, int32_t size);

void history_usbStart(void);

void history_usbFrame(uint8_t endPoint, uint8_t* buf, uint16_t size);

uint16_t history_getASCIIPackage(char* out, uint16_t outsize);

void history_disposeData(void);


#ifdef	__cplusplus
}
#endif

#endif	/* MESSAGEHISTORY_H */

