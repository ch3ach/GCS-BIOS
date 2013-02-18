#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/f4/gpio.h>
#include "msc.h"
#include "messagehistory.h"
#include "usbstorage.h"


static const struct usb_endpoint_descriptor msc_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x03,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = MSC_ENDPOINT_PACKAGE_SIZE,
        .bInterval = 1,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x83,
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
} _msc_header;

#pragma pack(pop)

int32_t cmdPointer = 0;
uint32_t cmdBuffer[256]; // <- 1024 bytes but alligned...

void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    uint8_t* buf = (uint8_t*) cmdBuffer;
    _msc_header* head=(_msc_header*)cmdBuffer;
    buf = &buf[cmdPointer];
    cmdPointer += usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);

    

    gpio_toggle(GPIOD, GPIO15);
}




