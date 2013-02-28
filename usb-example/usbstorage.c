#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/f4/gpio.h>
#include "msc.h"
#include "messagehistory.h"
#include "usbstorage.h"
#include "usbmanager.h"


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

static bool RXPackage = false;
static int32_t cmdLen = 0;
static uint32_t cmdBuffer[64]; // <- 256 bytes but alligned...

static inline msc_status_t msc_SCSIinquiry(uint32_t* buf, int32_t* len) {
    int32_t n;
    const uint8_t stdInqData[36] = {
        0x00, 0x80, 0x00, 0x01, 0x1f, 0x00, 0x00, 0x00, //8
        'C', 'B', 'I', '-', '>', 'G', 'C', 'S', //8
        'U', 'S', 'B', ' ', 'M', 'S', 'D', '-', 'A', 'c', 'c', 'e', 's', 's', ' ', ' ', //16
        '1', '.', '0', '0' //4
    };
    uint32_t* stdData = (uint32_t*) stdInqData;
    *len = sizeof (stdInqData);
    for (n = 0; n < 9/* 36 / 4 */; n++) {
        buf[n] = stdData[n];
    }

    return MSC_SEND_DATA;
}

static msc_status_t msc_tryExecuteSCSI(_msc_cbwheader_t* cmd, int32_t* cmdLen, int32_t cmdBufferSize) {
    (void) cmdBufferSize;
    if (*cmdLen <= 0)
        return MSC_STANDBY;

    switch (cmd->CB[0]) {
        case SCSI_TEST_UNIT_READY: return MSC_SEND_OK;
        case SCSI_REQUEST_SENSE: return MSC_SEND_FAIL;
        case SCSI_FORMAT_UNIT: return MSC_SEND_FAIL;
        case SCSI_INQUIRY: return msc_SCSIinquiry((uint32_t*) cmd, cmdLen);
        case SCSI_MODE_SELECT6: return MSC_SEND_FAIL;
        case SCSI_MODE_SENSE6: return MSC_SEND_FAIL;
        case SCSI_START_STOP_UNIT: return MSC_SEND_FAIL;
        case SCSI_MEDIA_REMOVAL: return MSC_SEND_FAIL;
        case SCSI_READ_FORMAT_CAPACITIES: return MSC_SEND_FAIL;
        case SCSI_READ_CAPACITY: return MSC_SEND_FAIL;
        case SCSI_READ10: return MSC_SEND_FAIL;
        case SCSI_WRITE10: return MSC_SEND_FAIL;
        case SCSI_VERIFY10: return MSC_SEND_FAIL;
        case SCSI_MODE_SELECT10: return MSC_SEND_FAIL;
        case SCSI_MODE_SENSE10: return MSC_SEND_FAIL;
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




