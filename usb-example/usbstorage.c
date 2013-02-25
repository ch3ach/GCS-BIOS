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

static int32_t cmdPointer = 0;
static uint32_t cmdBuffer[64]; // <- 256 bytes but alligned...
static uint32_t CSWTag;

static inline int32_t msc_SCSIinquiry(uint8_t* buf, int32_t len, uint8_t LUN) {
    (void) len;
    (void) LUN;
    int32_t n, ret = buf[4];
    const uint8_t stdInqData[36] = {
        0x00, 0x80, 0x00, 0x01, 0x1f, 0x00, 0x00, 0x00,
        'C', 'B', 'I', '-', '>', 'G', 'C', 'S',
        'U', 'S', 'B', ' ', 'M', 'S', 'D', '-', 'A', 'c', 'c', 'e', 's', 's', ' ', ' ',
        '1', '.', '0', '0'
    };
    uint32_t* stdData = (uint32_t*) stdInqData;
    for (n = 0; n < 9/* 36 / 4 */; n++) {
        cmdBuffer[n] = stdData[n];
    }

    return ret;
}

static int32_t msc_evaluateSCSI(uint8_t* buf, int32_t len, uint8_t LUN) {
    switch (buf[0]) {
        case SCSI_TEST_UNIT_READY: return 0;
        case SCSI_REQUEST_SENSE: return 0;
        case SCSI_FORMAT_UNIT: return 0;
        case SCSI_INQUIRY: return msc_SCSIinquiry(buf, len, LUN);
        case SCSI_MODE_SELECT6: return 0;
        case SCSI_MODE_SENSE6: return 0;
        case SCSI_START_STOP_UNIT: return 0;
        case SCSI_MEDIA_REMOVAL: return 0;
        case SCSI_READ_FORMAT_CAPACITIES: return 0;
        case SCSI_READ_CAPACITY: return 0;
        case SCSI_READ10: return 0;
        case SCSI_WRITE10: return 0;
        case SCSI_VERIFY10: return 0;
        case SCSI_MODE_SELECT10: return 0;
        case SCSI_MODE_SENSE10: return 0;
        default: return 0;
    }
}

void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    int32_t sendLen = -1;
    uint8_t* buf = (uint8_t*) cmdBuffer;
    _msc_cbwheader* head = (_msc_cbwheader*) cmdBuffer;
    buf = &buf[cmdPointer];
    cmdPointer += usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);
    gpio_toggle(GPIOD, GPIO15);

    if (head->Signature == MSC_CBW_Signature) {
        CSWTag = head->Tag;
        gpio_toggle(GPIOD, GPIO15);
        cmdPointer = 0;
        //sendLen = msc_evaluateSCSI(head->CB, head->CBLength, head->LUN);
    }

    if (sendLen > 0) {
        //sendLen -= usbmanager_send_packet(MSC_SENDING_EP, cmdBuffer, sendLen);
    }
}




