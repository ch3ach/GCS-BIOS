#include "SCSIUnknowns.h"

static uint8_t* buffer;
static int32_t bufferSize = -1;
static int32_t writePointer = 0;
static int32_t readPointer = 0;

#pragma pack(push, 1)

typedef union {
    uint32_t headerValue;

    struct {
        uint8_t size;
        uint8_t SCSICommand;
        uint8_t LUN;
        uint8_t CBLength;
    };
} scsiHeader_t;

#pragma pack(pop)

void SCSIUnknowns_init(void* buf, int32_t size) {
    uint32_t tmp = (uint32_t) buf;
    scsiHeader_t* header;
    if (0 != (tmp & 0x03)) {
        size -= tmp & 0x03;
        tmp = (tmp | 0x03) + 1;
        buf = (void*) tmp;
    }
    buffer = (uint8_t*) buf;
    bufferSize = size;
    writePointer = 0;

    header = (scsiHeader_t*) & buffer[writePointer];
    header->headerValue = 0;
}

void SCSIUnknowns_logPackage(uint8_t SCSICommand, uint8_t LUN, uint8_t CBLength, void* CBData) {
    if ((bufferSize - writePointer) > ((int32_t) CBLength + (int32_t)sizeof (scsiHeader_t))) {
        uint32_t* CB;
        uint32_t* dest;
        uint8_t n, m;
        scsiHeader_t* header = (scsiHeader_t*) & buffer[writePointer];
        header->size = CBLength + sizeof (scsiHeader_t);
        header->SCSICommand = SCSICommand;
        header->LUN = LUN;
        header->CBLength = CBLength;

        writePointer += sizeof (scsiHeader_t);

        CB = (uint32_t*) CBData;
        dest = (uint32_t*) & buffer[writePointer];
        for (n = 0, m = 0; n < CBLength; n += 4, m++) {
            dest[m] = CB[m];
        }
        writePointer += n;

        header = (scsiHeader_t*) & buffer[writePointer];
        header->headerValue = 0;
    }
}

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

static inline int32_t copyTextToBuffer(void* buffer, int32_t bufSize,
        const char* text, const int32_t textLen) {
    int32_t n, max;
    char* dest = (char*) buffer;

    max = textLen;
    if (bufSize < textLen)
        max = bufSize;

    for (n = 0; n < max; n++)
        dest[n] = text[n];

    return n;
}

static inline int32_t addTextToBuffer(void* buffer, const int32_t bufSize, const int32_t currentPos,
        const char* text, const int32_t textLen) {
    char* dest = (char*) buffer;
    int32_t size = bufSize - currentPos;

    dest = &dest[currentPos];

    return currentPos + copyTextToBuffer(dest, size, text, textLen);
}

int32_t SCSIUnknowns_nextASCII(void* out, int32_t outsize) {
    int32_t ret = 0;
    char* buf = (char*) out;
    scsiHeader_t* head = (scsiHeader_t*) & buffer[readPointer];

    if (((head->size & 0x03) != 0) || (head->size == 0)) {
        return 0;
    }

    readPointer += head->size;

    {
        const char text[] = "LUN: __\n";
        ret = addTextToBuffer(buf, outsize, ret, text, sizeof (text) - 1);

        if (outsize > ret) {
            ret += bin2hex(&buf[ret - 3], head->LUN);
        }
    }

    {
        const char text[] = "cmd: __\n";
        ret = addTextToBuffer(buf, outsize, ret, text, sizeof (text) - 1);

        if (outsize > ret) {
            ret += bin2hex(&buf[ret - 3], head->SCSICommand);
        }
    }

    {
        int32_t n;
        uint8_t* CBData = &buffer[readPointer];

        for (n = 0; n < head->CBLength && outsize > ret + 2; n++, readPointer += 4, ret += 2) {
            bin2hex(&buf[ret], CBData[n]);
        }
    }

    if (outsize > ret) {
        buf[ret] = '\n';
        ret++;
    }

    return ret;
}

void SCSIUnknowns_restartASCII(void) {
    readPointer = 0;
}

void SCSIUnknowns_disposeData(void) {
    writePointer = 0;
    readPointer = 0;
    buffer[writePointer] = 0x00;
}
