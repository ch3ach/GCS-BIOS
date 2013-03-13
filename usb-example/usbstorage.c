#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/f4/gpio.h>
#include "msc.h"
#include "messagehistory.h"
#include "usbstorage.h"
#include "usbmanager.h"

#include "ramdisk.h"


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
    //MSC_RECV_CMD,
    MSC_TRY_EXEC_CMD,
    MSC_RECV_DATA,
    MSC_SEND_DATA,
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
static uint32_t cmdBuffer[64]; // <- 256 bytes but alligned...
static msc_error_code_t errorCode = noError;
static uint8_t errorDesc[2] = {0, 0};

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

static inline msc_status_t msc_SCSIinquiry(uint32_t* buf, int32_t* len) {
    uint32_t n;
    const uint8_t stdInqData[36] = {
        0x00, 0x80, 0x00, 0x01, 0x1f, 0x00, 0x00, 0x00, //8
        'C', 'B', 'I', '-', '>', 'G', 'C', 'S', //8
        'U', 'S', 'B', ' ', 'M', 'S', 'D', '-', 'A', 'c', 'c', 'e', 's', 's', ' ', ' ', //16
        '1', '.', '0', '0' //4
    };
    uint32_t* stdData = (uint32_t*) stdInqData;
    *len = sizeof (stdInqData);
    for (n = 0; n < ((sizeof (stdInqData) + 3) >> 2)/* 36 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static inline msc_status_t msc_SCSIsense6(uint32_t* buf, int32_t* len) {
    uint32_t n;
    const uint8_t stdSenseData[36] = {
        0x23, 0x00, 0x00, 0x00, 0x05, 0x1e, 0xf0, 0x00, //8
        0xff, 0x20, 0x02, 0x00, 0x3f, 0x07, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //8
        0x00, 0x00, 0x00, 0x00 //4
    }; // */
    /*const uint8_t stdSenseData[8] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //8
    }; // */
    uint32_t* stdData = (uint32_t*) stdSenseData;
    *len = sizeof (stdSenseData);
    for (n = 0; n < ((sizeof (stdSenseData) + 3) >> 2)/* 36 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static inline msc_status_t msc_SCSIsense10(uint32_t* buf, int32_t* len) {
    uint32_t n;
    const uint8_t stdSenseData[8] = {
        0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 //8
    };
    uint32_t* stdData = (uint32_t*) stdSenseData;
    *len = sizeof (stdSenseData);
    for (n = 0; n < ((sizeof (stdSenseData) + 3) >> 2)/* 8 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static msc_status_t msc_testUnitReady(_msc_cbwheader_t* cmd) {
    switch (cmd->LUN) {
        case 0:
            if (ramdisk_ready())
                return MSC_SEND_OK;
            break;
    }
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

static msc_status_t msc_tryExecuteSCSI(_msc_cbwheader_t* cmd, int32_t* cmdLen, int32_t cmdBufferSize) {
    (void) cmdBufferSize;
    if (*cmdLen < (int32_t)sizeof (_msc_cbwheader_t))
        return MSC_STANDBY;

    errorCode = illegalRequest;
    errorDesc[0] = 0x20;
    errorDesc[1] = 0;

    switch (cmd->CB[0]) {
        case SCSI_TEST_UNIT_READY: return msc_testUnitReady(cmd);
        case SCSI_REQUEST_SENSE: return msc_SCSIsense((uint32_t*) cmd, cmdLen);
        case SCSI_FORMAT_UNIT: return MSC_SEND_FAIL;
        case SCSI_INQUIRY: return msc_SCSIinquiry((uint32_t*) cmd, cmdLen);
        case SCSI_MODE_SELECT6: return MSC_SEND_FAIL;
        case SCSI_MODE_SENSE6: return msc_SCSIsense6((uint32_t*) cmd, cmdLen);
        case SCSI_START_STOP_UNIT: return MSC_SEND_FAIL;
        case SCSI_MEDIA_REMOVAL: return MSC_SEND_FAIL;
        case SCSI_READ_FORMAT_CAPACITIES: return MSC_SEND_FAIL;
        case SCSI_READ_CAPACITY: return msc_readCapacity(cmd, cmdLen);
        case SCSI_READ10: return MSC_SEND_FAIL;
        case SCSI_WRITE10: return MSC_SEND_FAIL;
        case SCSI_VERIFY10: return MSC_SEND_FAIL;
        case SCSI_MODE_SELECT10: return MSC_SEND_FAIL;
        case SCSI_MODE_SENSE10: return msc_SCSIsense10((uint32_t*) cmd, cmdLen);
        default: return MSC_SEND_FAIL;
    }
}

void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    uint8_t* buf = (uint8_t*) cmdBuffer;

    gpio_set(GPIOD, GPIO15);

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
            history_usbFrame(MSC_RECEIVING_EP, cmdBuffer, cmdLen);
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
                history_usbFrame(MSC_SENDING_EP, buf, cmdLen - cmdSent);
                cmdSent += usbmanager_send_packet(MSC_SENDING_EP, buf, cmdLen - cmdSent);
            } else {
                state = MSC_SEND_OK;
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
                gpio_clear(GPIOD, GPIO15);
            }
        }
            break;
    }
}




