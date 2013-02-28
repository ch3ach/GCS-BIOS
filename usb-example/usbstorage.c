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
} _msc_cbwheader;

#pragma pack(pop)

typedef enum {
    MSC_STANDBY = 0,
    //MSC_RECV_CMD,
    MSC_TRY_EXEC_CMD,
    MSC_RECV_DATA,
    MSC_SEND_DATA,
    MSC_SEND_OK,
    MSC_SEND_ERROR
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

static msc_status_t msc_tryExecuteSCSI(_msc_cbwheader* cmd, int32_t* cmdLen, int32_t cmdBufferSize) {
    (void) cmdBufferSize;
    if (*cmdLen <= 0)
        return MSC_STANDBY;

    switch (cmd->CB[0]) {
        case SCSI_TEST_UNIT_READY: return MSC_SEND_ERROR;
        case SCSI_REQUEST_SENSE: return MSC_SEND_ERROR;
        case SCSI_FORMAT_UNIT: return MSC_SEND_ERROR;
        case SCSI_INQUIRY: return msc_SCSIinquiry((uint32_t*) cmd, cmdLen);
        case SCSI_MODE_SELECT6: return MSC_SEND_ERROR;
        case SCSI_MODE_SENSE6: return MSC_SEND_ERROR;
        case SCSI_START_STOP_UNIT: return MSC_SEND_ERROR;
        case SCSI_MEDIA_REMOVAL: return MSC_SEND_ERROR;
        case SCSI_READ_FORMAT_CAPACITIES: return MSC_SEND_ERROR;
        case SCSI_READ_CAPACITY: return MSC_SEND_ERROR;
        case SCSI_READ10: return MSC_SEND_ERROR;
        case SCSI_WRITE10: return MSC_SEND_ERROR;
        case SCSI_VERIFY10: return MSC_SEND_ERROR;
        case SCSI_MODE_SELECT10: return MSC_SEND_ERROR;
        case SCSI_MODE_SENSE10: return MSC_SEND_ERROR;
        default: return MSC_SEND_ERROR;
    }
}

void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    uint8_t* buf = (uint8_t*) cmdBuffer;

    gpio_set(GPIOD, GPIO15);

    buf = &buf[cmdLen];
    cmdLen += usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);
    RXPackage = true;


    /*int32_t sendLen = -1;
    uint8_t* buf = (uint8_t*) cmdBuffer;
    _msc_cbwheader* head = (_msc_cbwheader*) cmdBuffer;
    buf = &buf[cmdPointer];
    cmdPointer += usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);
    

    if (head->Signature == MSC_CBW_Signature) {
        CSWTag = head->Tag;
        gpio_toggle(GPIOD, GPIO15);
        cmdPointer = 0;
        sendLen = msc_evaluateSCSI(head->CB, head->CBLength, head->LUN);
    }

    if (sendLen > 0) {
        sendLen -= usbmanager_send_packet(MSC_SENDING_EP, cmdBuffer, sendLen);
    }//*/
}

void msc_stateMachine(usbd_device *usbd_dev) {
    static msc_status_t state = MSC_STANDBY;
    static int32_t cmdSent = 0;
    static uint32_t CSWTag;

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
            _msc_cbwheader* head = (_msc_cbwheader*) cmdBuffer;
            history_usbFrame(MSC_RECEIVING_EP, cmdBuffer, cmdLen);
            CSWTag = head->Tag;
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
        case MSC_SEND_ERROR:
            cmdLen = 0;
            cmdSent = 0;
            state = MSC_STANDBY;
            gpio_clear(GPIOD, GPIO15);
            break;
    }
}




