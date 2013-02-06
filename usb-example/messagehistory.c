#include "messagehistory.h"

static uint8_t* histbuffer;
static int32_t histsize = -1;
static int32_t histpointer = 0;
static int32_t histreader = 0;

#pragma pack(push, 1)

typedef struct {
    uint8_t opCode;
    uint8_t endPoint;
    uint16_t size;
} histheader_t;

#pragma pack(pop)

typedef enum {
    STOP_MARKER = 0,
    USB_START = 1,
    USB_FRAME
} historyCmd_t;

void history_init(uint8_t* buffer, int32_t size) {
    int32_t tmp = (int32_t) buffer;
    if (0 != (tmp & 0x03)) {
        size -= tmp & 0x03;
        tmp = (tmp | 0x03) + 1;
        buffer = (uint8_t*) tmp;
    }
    histbuffer = buffer;
    histsize = size;
    histpointer = 0;

    histbuffer[histpointer] = STOP_MARKER;
}

void history_usbStart(void) {
    histheader_t* head = (histheader_t*) & histbuffer[histpointer];

    if ((histpointer + sizeof (histheader_t)) >= histsize)
        return;

    head->opCode = USB_START;
    head->endPoint = 0;
    head->size = 0;
    histpointer += sizeof (histheader_t);
}

void history_usbFrame(uint8_t endPoint, uint8_t* buf, uint16_t size) {
    uint16_t n;
    histheader_t* head = (histheader_t*) & histbuffer[histpointer];

    if ((histpointer + sizeof (histheader_t) + size) >= histsize)
        return;

    head->opCode = USB_FRAME;
    head->endPoint = endPoint;
    head->size = size;
    histpointer += sizeof (histheader_t);

    for (n = 0; n < size; n++) {
        histbuffer[histpointer + n] = buf[n];
    }

    size += 0x03;
    size &= ~0x03;
    histpointer += size;

    histbuffer[histpointer] = STOP_MARKER;
}

uint16_t history_getASCIIPackage(uint8_t* out, uint16_t outsize) {
    uint16_t n, ret = 0;
    histheader_t* head = (histheader_t*) & histbuffer[histreader];

    switch (head->opCode) {
        case STOP_MARKER:
            histreader = 0;
            history_disposeData();
            break;
        case USB_START:
        {
            const char* startUSB = "Start USB\n";
            ret = 10;
            if (ret > outsize)
                ret = outsize;
            memcpy(out, startUSB, ret);
        }
            break;
        case USB_FRAME:
        {
            
        }
        break;
        default:
        {
            const char* unknownError = "ERROR: Unknown Package\n";
            ret = 23;
            if (ret > outsize)
                ret = outsize;
            memcpy(out, unknownError, ret);
        }
            break;
    }

    return ret;
}

void history_disposeData(void) {
    histpointer = 0;
    histbuffer[histpointer] = STOP_MARKER;
}
