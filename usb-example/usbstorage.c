#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/f4/gpio.h>
#include "msc.h"
#include "messagehistory.h"
#include "usbstorage.h"


static const struct usb_endpoint_descriptor msc_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x04,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = MSC_ENDPOINT_PACKAGE_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x85,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = MSC_ENDPOINT_PACKAGE_SIZE,
	.bInterval = 1,
}};

const struct usb_interface_descriptor msc_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 2,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_DEVICE_CLASS_STORAGE,
	.bInterfaceSubClass = MSC_SUBCLASS_SCSI,
	.bInterfaceProtocol = MSC_PROTOCOL_BULK_ONLY,
	.iInterface = 0x64,

	.endpoint = msc_endp,       
}};


void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    uint8_t buf[MSC_ENDPOINT_PACKAGE_SIZE];
    int len = usbd_ep_read_packet(usbd_dev, ep, buf, MSC_ENDPOINT_PACKAGE_SIZE);
    
    history_usbFrame(ep, buf, len);

    gpio_toggle(GPIOD, GPIO15);
}




