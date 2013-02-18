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
    USB_FRAME,
    USB_CONTROL
} historyCmd_t;

int32_t bin2hex(char* buf, const uint8_t data) {
    char tmp;
    tmp = data;
    tmp >>= 4;
    tmp += '0';
    if (tmp > '9')
        tmp += 'A' - '9' - 1;
    buf[0] = tmp;

    tmp = data;
    tmp &= 0x0F;
    tmp += '0';
    if (tmp > '9')
        tmp += 'A' - '9' - 1;
    buf[1] = tmp;

    return 2;
}

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

    if ((histpointer + (int32_t)sizeof (histheader_t)) >= histsize)
        return;

    head->opCode = USB_START;
    head->endPoint = 0;
    head->size = 0;
    histpointer += sizeof (histheader_t);
}

void history_usbFrame(uint8_t endPoint, uint8_t* buf, uint16_t size) {
    uint16_t n;
    histheader_t* head = (histheader_t*) & histbuffer[histpointer];

    if ((histpointer + (int32_t)sizeof (histheader_t) + size) >= histsize)
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

void history_usbControl(void* controlBuf, uint8_t controlSize, uint8_t* dataBuf, uint16_t dataSize) {
    uint16_t n, m;
    histheader_t* head = (histheader_t*) & histbuffer[histpointer];
    uint8_t* cBuf = (uint8_t*) controlBuf;

    if ((histpointer + (int32_t)sizeof (histheader_t) + controlSize + dataSize) >= histsize)
        return;

    head->opCode = USB_CONTROL;
    head->endPoint = controlSize;
    head->size = controlSize + dataSize;
    histpointer += sizeof (histheader_t);

    for (n = 0; n < controlSize; n++) {
        histbuffer[histpointer + n] = cBuf[n];
    }
    m = n;
    for (n = 0; n < dataSize; n++, m++) {
        histbuffer[histpointer + m] = dataBuf[n];
    }

    m += 0x03;
    m &= ~0x03;
    histpointer += m;

    histbuffer[histpointer] = STOP_MARKER;
}

uint16_t history_getASCIIPackage(char* out, uint16_t outsize) {
    uint16_t add, ret = 0;
    char* buf = (char*) out;
    histheader_t* head = (histheader_t*) & histbuffer[histreader];

    histreader += sizeof (histheader_t);

    switch (head->opCode) {
        case STOP_MARKER:
            histreader = 0;
            history_disposeData();
            return 0;
        case USB_START:
        {
            const char msg[] = "Start USB\n";
            const uint16_t msgLen = sizeof (msg);
            ret = msgLen - 1;
            if (ret > outsize)
                ret = outsize;
            memcpy(buf, msg, ret);
        }
            break;
        case USB_FRAME:
        {
            uint16_t n;
            uint8_t* data = &histbuffer[histreader];

            if (outsize > 2) {
                ret += bin2hex(buf, head->endPoint);
            }
            if (outsize > 3) {
                buf[ret] = ':';
                ret++;
            }
            if (outsize > 4) {
                buf[ret] = ' ';
                ret++;
            }
            for (n = 0; (n < head->size) && ((ret + 3) < outsize); n++) {
                ret += bin2hex(&buf[ret], data[n]);
                buf[ret] = ' ';
                ret++;
            }

            buf[ret - 1] = '\n';
        }
            break;
        case USB_CONTROL:
        {
            const char msg[] = "control: ";
            const uint16_t msgLen = sizeof (msg);
            uint16_t n;
            uint8_t* data = &histbuffer[histreader];

            if (outsize > msgLen) {
                ret = msgLen - 1;
                if (ret > outsize)
                    ret = outsize;
                memcpy(buf, msg, ret);
            }
            for (n = 0; (n < head->endPoint) && (ret + 3 < outsize); n++) {
                ret += bin2hex(&buf[ret], data[n]);
                buf[ret] = ' ';
                ret++;
            }
            if (outsize > ret + 2) {
                buf[ret] = '|';
                ret++;
                buf[ret] = ' ';
                ret++;
            }
            for (; (n < head->size) && (ret + 3 < outsize); n++) {
                ret += bin2hex(&buf[ret], data[n]);
                buf[ret] = ' ';
                ret++;
            }

            buf[ret - 1] = '\n';
        }
            break;
        default:
        {
            const char msg[] = "ERROR: Unknown Package\n";
            const uint16_t msgLen = sizeof (msg);
            ret = msgLen - 1;
            if (ret > outsize)
                ret = outsize;
            memcpy(buf, msg, ret);
        }
            break;
    }

    add = head->size;
    add += 0x03;
    add &= ~0x03;

    histreader += add;

    if (histreader >= histpointer) {
        histreader = 0;
        history_disposeData();
    }

    return ret;
}

void history_disposeData(void) {
    histpointer = 0;
    histbuffer[histpointer] = STOP_MARKER;
}
