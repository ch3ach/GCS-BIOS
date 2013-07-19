#include <libopencm3/usb/usbd.h>
#include "msc.h"
#include "messagehistory.h"
#include "usbstorage.h"
#include "usbmanager.h"

#include "ramdisk.h"
#include "led.h"


static const struct usb_endpoint_descriptor msc_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = MSC_RECEIVING_EP,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = MSC_ENDPOINT_PACKAGE_SIZE,
        .bInterval = 1,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = MSC_SENDING_EP,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = MSC_ENDPOINT_PACKAGE_SIZE,
        .bInterval = 1,
    }
};

const struct usb_interface_descriptor msc_iface[] = {
    {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 2,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_DEVICE_CLASS_STORAGE,
        .bInterfaceSubClass = MSC_SUBCLASS_SCSI,
        .bInterfaceProtocol = MSC_PROTOCOL_BULK_ONLY,
        .iInterface = 0x04,

        .endpoint = msc_endp,
    }
};

#pragma pack(push, 1)

typedef struct {
    uint32_t Signature;
    uint32_t Tag;
    uint32_t DataLength;
    uint8_t Flags;
    uint8_t LUN;
    uint8_t CBLength;
    uint8_t CB[16];
} _msc_cbwheader_t;

typedef struct {
    uint32_t Signature;
    uint32_t Tag;
    uint32_t DataResidue;
    uint8_t Status;
} _msc_cswheader_t;

#pragma pack(pop)

#define MSC_CBW_Signature               0x43425355
#define MSC_CSW_Signature               0x53425355

/* CSW Status Definitions */
#define CSW_CMD_PASSED                  0x00
#define CSW_CMD_FAILED                  0x01
#define CSW_PHASE_ERROR                 0x02

typedef enum {
    MSC_STANDBY = 0,
    MSC_TRY_EXEC_CMD,
    MSC_RECV_DATA,
    MSC_SEND_DATA,
    MSC_READ_DATA,
    MSC_WRITE_DATA,
    MSC_SEND_OK,
    MSC_SEND_FAIL,
    MSC_SEND_PHASE_ERROR
} msc_status_t;

typedef enum {
    noError = 0,
    recovered, /*RECOVERED ERROR. Indicates that the last command completed successfully
with some recovery action performed by the UFI device. Details may be
determinable by examining the additional sense bytes and the Information field.
When multiple recovered errors occur during one command, the choice of which
error to report is device specific.*/
    notReady, /*NOT READY. Indicates that the UFI device cannot be accessed. Operator
intervention may be required to correct this condition.*/
    mediumError, /*MEDIUM ERROR. Indicates that the command terminated with a non-recovered
error condition that was probably caused by a flaw in the medium or an error in the
recorded data. This sense key may also be returned if the UFI device is unable to
distinguish between a flaw in the medium and a specific hardware failure (sense key
4h).*/
    hardwareError, /*HARDWARE ERROR. Indicates that the UFI device detected a non-recoverable
hardware failure while performing the command or during a self test.*/
    illegalRequest, /*ILLEGAL REQUEST. Indicates that there was an illegal parameter in the Command
Packet or in the additional parameters supplied as data for some commands. If the
UFI device detects an invalid parameter in the Command Packet, then it shall
terminate the command without altering the medium. If the UFI device detects an
invalid parameter in the additional parameters supplied as data, then the UFI device
may have already altered the medium.*/
    unitAttention, /*UNIT ATTENTION. Indicates that the removable medium may have been changed
or the UFI device has been reset.*/
    dataProtect, /*DATA PROTECT. Indicates that a command that writes the medium was attempted
on a block that is protected from this operation. The write operation was not
performed.*/
    blankCheck, /*BLANK CHECK. Indicates that a write-once device or a sequential-access device
encountered blank medium or format-defined end-of-data indication while reading or
a write-once device encountered a non-blank medium while writing.*/
    vendorError, /*Vendor Specific. This sense key is available for reporting vendor specific conditions.*/
    Reserved0,
    abortedCommand, /*ABORTED COMMAND. Indicates that the UFI device has aborted the command.
The host may be able to recover by trying the command again.*/
    Reserved1,
    volumeOverflow, /*VOLUME OVERFLOW. Indicates that a buffered peripheral device has reached the
end-of-partition and data may remain in the buffer that has not been written to the
medium.*/
    miscompare, /*MISCOMPARE. Indicates that the source data did not match the data read from the
medium.*/
    Reserved2
} msc_error_code_t;

static bool RXPackage = false;
static int32_t cmdLen = 0;
static uint32_t cmdBuffer[128 / sizeof (uint32_t)];
static msc_error_code_t errorCode = noError;
static uint8_t errorDesc[2] = {0, 0};
static uint8_t unitReady = 0;

static void change_endian(void* point, int32_t len) {
    int32_t n;
    uint8_t tmp;
    uint8_t* buf = (uint8_t*) point;
    for (n = 0; n < (len / 2); n++) {
        tmp = buf[n];
        buf[n] = buf[len - n - 1];
        buf[len - n - 1] = tmp;
    }
}

static inline msc_status_t msc_inquiry(uint32_t* buf, int32_t* len) {
    uint32_t n;
    const uint8_t stdInqData[36] = {
        0x00, 0x80, 0x00, 0x01, 0x1f, 0x00, 0x00, 0x00, //8
        'U', 'S', 'B', ' ', 'D', 'I', 'S', 'K', //8
        'C', 'B', 'I', '-', '>', 'G', 'C', 'S', '-', '>', 'M', 'S', 'C', ' ', ' ', ' ', //16
        'P', 'M', 'A', 'P' //4
    };
    uint32_t* stdData = (uint32_t*) stdInqData;
    *len = sizeof (stdInqData);
    for (n = 0; n < ((sizeof (stdInqData) + 3) >> 2)/* 36 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static inline msc_status_t msc_sense6(_msc_cbwheader_t* cmd, int32_t* len) {
    uint32_t n;
    uint8_t stdSenseData[36] = {
        0x23, 0x00, 0x00, 0x00, 0x05, 0x1e, 0xf0, 0x00, //8
        0xff, 0x20, 0x02, 0x00, 0x01, 0xe1, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00 //4
    };
    uint8_t* SectpCylin = &stdSenseData[9];
    uint16_t* SectSize = (uint16_t*) & stdSenseData[10];
    uint16_t* CylinCount = (uint16_t*) & stdSenseData[12];

    uint32_t* buf = (uint32_t*) cmd;
    uint32_t* stdData = (uint32_t*) stdSenseData;

    switch (cmd->LUN) {
        case 0:
            *SectSize = (uint16_t) ramdisk_getBlockSize();
            *CylinCount = (uint16_t) (ramdisk_getBlockCount() / (*SectpCylin));
            break;
        default:
            errorCode = illegalRequest;
            errorDesc[0] = 0x25;
            errorDesc[1] = 0;
            return MSC_SEND_FAIL;
    }

    change_endian(SectSize, sizeof (uint16_t));
    change_endian(CylinCount, sizeof (uint16_t));

    *len = sizeof (stdSenseData);
    for (n = 0; n < ((sizeof (stdSenseData) + 3) >> 2)/* 36 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

//static inline msc_status_t msc_SCSIsense10(uint32_t* buf, int32_t* len) {
//    uint32_t n;
//    const uint8_t stdSenseData[36] = {
//        0x23, 0x00, 0x00, 0x00, 0x05, 0x1e, 0xf0, 0x00, //8
//        0xff, 0x20, 0x02, 0x00, 0x01, 0xe1, 0x00, 0x00, //8
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
//        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
//        0x00, 0x00, 0x00, 0x00 //4
//    }; /*/
//    const uint8_t stdSenseData[8] = {
//        0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //8
//    }; // */
//    uint32_t* stdData = (uint32_t*) stdSenseData;
//    *len = sizeof (stdSenseData);
//    for (n = 0; n < ((sizeof (stdSenseData) + 3) >> 2)/* 8 / 4 */; n++) {
//        buf[n] = stdData[n];
//    }
//
//    return MSC_SEND_DATA;
//}

static msc_status_t msc_testUnitReady(_msc_cbwheader_t* cmd) {
    if (0 != unitReady) {
        switch (cmd->LUN) {
            case 0:
                if (ramdisk_ready())
                    return MSC_SEND_OK;
                break;
        }

        errorCode = illegalRequest;
        errorDesc[0] = 0x25;
        errorDesc[1] = 0;
    } else {
        unitReady = 1;
        errorCode = unitAttention; //should be 0x06
        errorDesc[0] = 0x28;
        errorDesc[1] = 0;
    }

    return MSC_SEND_FAIL;
}

static inline msc_status_t msc_SCSIsense(uint32_t* buf, int32_t* len) {
    uint32_t n;
    uint8_t stdSenseData[18] = {
        0x70, /* bit 7: validbit; bit 6-0: FixValue 70h */
        0x00, /* reserved */
        0x00, /* bit 7-4: reserved; bit 3-0: senseKey */
        0x00, 0x00, 0x00, 0x00, /* 4-byte Information */
        10, /* Additional Sense Length (must be 10) */
        0x00, 0x00, 0x00, 0x00, /* reserved 4-bytes */
        0x00, /* additional sense code (mandatory) */
        0x00, /* additional sense code qualifyer (mandatory) */
        0x00, 0x00, 0x00, 0x00 /* reserved 4-bytes */
    };

    stdSenseData[2] = errorCode;
    stdSenseData[12] = errorDesc[0];
    stdSenseData[13] = errorDesc[1];

    uint32_t* stdData = (uint32_t*) stdSenseData;
    *len = sizeof (stdSenseData);
    for (n = 0; n < ((sizeof (stdSenseData) + 3) >> 2)/* 8 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static inline msc_status_t msc_mediaRemoval(const uint8_t* cmd, const int8_t cmdlen) {
    (void) cmdlen;
    if (0 == cmd[4])
        return MSC_SEND_OK;

    errorCode = illegalRequest;
    errorDesc[0] = 0x24;
    errorDesc[1] = 0;

    return MSC_SEND_FAIL;
}

static msc_status_t msc_readCapacity(_msc_cbwheader_t* cmd, int32_t* cmdLen) {
    uint32_t* values = (uint32_t*) cmd;
    switch (cmd->LUN) {
        case 0:
            values[0] = ramdisk_getBlockCount();
            values[1] = ramdisk_getBlockSize();
            change_endian(&values[0], sizeof (uint32_t));
            change_endian(&values[1], sizeof (uint32_t));
            *cmdLen = 8;

            return MSC_SEND_DATA;
    }
    return MSC_SEND_FAIL;
}

static uint32_t readAddr;
static uint32_t readCount;
static uint8_t readLUN;

static inline msc_status_t msc_read10(_msc_cbwheader_t* cmd, int32_t* cmdLen) {
    uint8_t* data = cmd->CB;
    uint16_t readBlocks = *((uint16_t*) & (data[7]));
    readAddr = *((uint32_t*) & (data[2]));
    change_endian(&readAddr, sizeof (uint32_t));
    change_endian(&readBlocks, sizeof (uint16_t));
    readLUN = cmd->LUN;
    switch (readLUN) {
        case 0:
            *cmdLen = 0;
            readAddr *= ramdisk_getBlockSize();
            readCount = readBlocks * ramdisk_getBlockSize();
            return MSC_READ_DATA;
    }
    return MSC_SEND_FAIL;
}

static uint32_t writeAddr;
static uint32_t writeCount;
static uint8_t writeLUN;

static inline msc_status_t msc_write10(_msc_cbwheader_t* cmd, int32_t* cmdLen) {
    uint8_t* data = cmd->CB;
    uint16_t writeBlocks = *((uint16_t*) & (data[7]));
    writeAddr = *((uint32_t*) & (data[2]));
    change_endian(&writeAddr, sizeof (uint32_t));
    change_endian(&writeBlocks, sizeof (uint16_t));
    writeLUN = cmd->LUN;
    switch (writeLUN) {
        case 0:
            *cmdLen = 0;
            writeAddr *= ramdisk_getBlockSize();
            writeCount = writeBlocks * ramdisk_getBlockSize();
            return MSC_WRITE_DATA;
    }
    return MSC_SEND_FAIL;
}

static msc_status_t msc_tryExecuteSCSI(_msc_cbwheader_t* cmd, int32_t* cmdLen, int32_t cmdBufferSize) {
    (void) cmdBufferSize;
    if (*cmdLen < (int32_t)sizeof (_msc_cbwheader_t))
        return MSC_STANDBY;

    switch (cmd->CB[0]) {
        case SCSI_TEST_UNIT_READY: return msc_testUnitReady(cmd);
        case SCSI_REQUEST_SENSE: return msc_SCSIsense((uint32_t*) cmd, cmdLen);
        case SCSI_FORMAT_UNIT: break;
        case SCSI_INQUIRY: return msc_inquiry((uint32_t*) cmd, cmdLen);
        case SCSI_MODE_SELECT6: break;
        case SCSI_MODE_SENSE6: return msc_sense6(cmd, cmdLen);
        case SCSI_START_STOP_UNIT: break;
        case SCSI_MEDIA_REMOVAL: return msc_mediaRemoval(cmd->CB, cmd->CBLength);
        case SCSI_READ_FORMAT_CAPACITIES: break;
        case SCSI_READ_CAPACITY: return msc_readCapacity(cmd, cmdLen);
        case SCSI_READ10: return msc_read10(cmd, cmdLen);
        case SCSI_WRITE10: return msc_write10(cmd, cmdLen);
        case SCSI_VERIFY10: break;
        case SCSI_MODE_SELECT10: break;
        case SCSI_MODE_SENSE10: break;
        default: break;
    }

    errorCode = illegalRequest;
    errorDesc[0] = 0x20;
    errorDesc[1] = 0;

    return MSC_SEND_FAIL;
}

void msc_resetLUNs(void) {
    unitReady = 0;
}

void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {

    uint8_t* buf = (uint8_t*) cmdBuffer;

    led_blue_toggle();

    buf = &buf[cmdLen];
    cmdLen += usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);
    RXPackage = true;
}

void msc_stateMachine(usbd_device *usbd_dev) {
    (void) usbd_dev;
    static msc_status_t state = MSC_STANDBY;
    static int32_t cmdSent = 0;
    static uint32_t CSWTag;
    static uint32_t TransferLen;

    switch (state) {
        case MSC_STANDBY:
            if (RXPackage) {
                state = MSC_TRY_EXEC_CMD;
                RXPackage = false;
            } else {
                break;
            }
        case MSC_TRY_EXEC_CMD:
        {
            _msc_cbwheader_t* head = (_msc_cbwheader_t*) cmdBuffer;
            CSWTag = head->Tag;
            TransferLen = head->DataLength;
            state = msc_tryExecuteSCSI(head, &cmdLen, sizeof (cmdBuffer));
        }
            break;
        case MSC_RECV_DATA: state = MSC_SEND_OK;
            break;
        case MSC_SEND_DATA:
        {
            uint8_t* buf = (uint8_t*) cmdBuffer;
            buf = &buf[cmdSent];
            if (cmdLen > cmdSent) {
                cmdSent += usbmanager_send_packet(MSC_SENDING_EP, buf, cmdLen - cmdSent);
            } else {
                state = MSC_SEND_OK;
            }
        }
            break;
        case MSC_READ_DATA:
        {
            uint8_t* buf = (uint8_t*) cmdBuffer;

            if ((uint32_t) cmdSent == readCount) {
                state = MSC_SEND_OK;
            } else {
                if (cmdLen > cmdSent) {
                    cmdSent += usbmanager_send_packet(MSC_SENDING_EP, buf, cmdLen - cmdSent);
                }
                if (cmdLen == cmdSent) {
                    uint32_t len = readCount - cmdSent;
                    if (len > MSC_ENDPOINT_PACKAGE_SIZE)
                        len = MSC_ENDPOINT_PACKAGE_SIZE;
                    len = ramdisk_read(readAddr, cmdBuffer, len);
                    cmdLen += len;
                    readAddr += len;
                }
            }
        }
            break;
        case MSC_WRITE_DATA:
        {
            if (RXPackage) {
                uint32_t len;

                len = ramdisk_write(writeAddr, cmdBuffer, cmdLen);
                cmdLen = 0;
                RXPackage = false;
                cmdSent += len;
                writeAddr += len;
                if ((uint32_t)cmdSent == writeCount) {
                    state = MSC_SEND_OK;
                }
            }
        }
            break;
        case MSC_SEND_OK:
            errorCode = noError;
            errorDesc[0] = 0;
            errorDesc[1] = 0;
        case MSC_SEND_FAIL:
        case MSC_SEND_PHASE_ERROR:
        {
            _msc_cswheader_t* head = (_msc_cswheader_t*) cmdBuffer;
            head->Signature = MSC_CSW_Signature;
            head->Tag = CSWTag;
            head->DataResidue = TransferLen - cmdSent;
            head->Status = CSW_CMD_PASSED;
            if (state == MSC_SEND_FAIL)
                head->Status = CSW_CMD_FAILED;
            else if (state == MSC_SEND_PHASE_ERROR)
                head->Status = CSW_PHASE_ERROR;

            if (sizeof (_msc_cswheader_t) == usbmanager_send_packet(MSC_SENDING_EP, cmdBuffer, sizeof (_msc_cswheader_t))) {
                cmdLen = 0;
                cmdSent = 0;
                state = MSC_STANDBY;
            }
        }
            break;
        default:
            errorCode = illegalRequest;
            errorDesc[0] = 0x20;
            errorDesc[1] = 0;
            state = MSC_SEND_FAIL;
            break;
    }
}




