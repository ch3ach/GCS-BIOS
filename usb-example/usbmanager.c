#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include "SCSIUnknowns.h"
#include "usbmanager.h"
#include "usbstorage.h"
#include "cdcacm.h"
#include "msc.h"
#include "led.h"


static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_CLASS_MISCELLANEOUS,
    .bDeviceSubClass = 0x02,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = USBMANAGER_FIFO0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x5740,
    .bcdDevice = 0x0200,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const struct usb_interface ifaces[] = {
    {
        .num_altsetting = 1,
        .altsetting = comm_iface,
    },
    {
        .num_altsetting = 1,
        .altsetting = data_iface,
    },
    {
        .num_altsetting = 1,
        .altsetting = msc_iface,
    }
};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 3,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = USB_CONFIG_ATTR_BUS_POWERED,
    .bMaxPower = USB_CONFIG_POWER_MA(500), //0xFA = 500mA, 0x32 = 100mA

    .interface = ifaces,
};

static const char *usb_strings[] = {
    "CB Innovations", /* Index 0x01: Manufacturer */
    "GCS BIOS USB Test", /* Index 0x02: Product */
    "GCS Test", /* Index 0x03: Serial Number */
};

static usbd_device *usbd_dev;
static uint32_t controlBuffer[(128 / sizeof (uint32_t))];
static uint16_t tx_ep_sizes[5];

static int usbmanager_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, u8 **buf,
        u16 *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req)) {
    (void) complete;
    (void) buf;
    (void) usbd_dev;

    switch (req->bRequest) {
        case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
        {
            /*
             * This Linux cdc_acm driver requires this to be implemented
             * even though it's optional in the CDC spec, and we don't
             * advertise it in the ACM functional descriptor.
             */
            return USBD_REQ_HANDLED;
        }
        case USB_CDC_REQ_SET_LINE_CODING:
        {
            if (*len < sizeof (struct usb_cdc_line_coding))
                return USBD_REQ_NOTSUPP;

            return USBD_REQ_HANDLED;
        }
        case MSC_REQUEST_GET_MAX_LUN:
        {
            *buf[0] = MSC_IMPLEMENTED_LUNS - 1;
            *len = 1;
            msc_resetLUNs();
            return USBD_REQ_HANDLED;
        }
    }
    return USBD_REQ_NOTSUPP;
}

/*
static void usbmanager_data_rx(usbd_device *usbd_dev, u8 ep) {
    switch (ep) {
        case CDC_RECEIVING_EP: cdcacm_data_rx_cb(usbd_dev, ep);
            break;
        case MSC_RECEIVING_EP: msc_data_rx_cb(usbd_dev, ep);
            break;
    }
}
*/

static void usbmanager_set_config(usbd_device *usbd_dev, u16 wValue) {
    (void) wValue;

    usbd_ep_setup(usbd_dev, CDC_RECEIVING_EP, USB_ENDPOINT_ATTR_BULK, CDC_ENDPOINT_PACKAGE_SIZE, cdcacm_data_rx_cb);
    usbd_ep_setup(usbd_dev, CDC_SENDING_EP, USB_ENDPOINT_ATTR_BULK, CDC_ENDPOINT_PACKAGE_SIZE, NULL);
    usbd_ep_setup(usbd_dev, CDC_INTERRUPT_EP, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);
    usbd_ep_setup(usbd_dev, MSC_RECEIVING_EP, USB_ENDPOINT_ATTR_BULK, MSC_ENDPOINT_PACKAGE_SIZE, msc_data_rx_cb);
    usbd_ep_setup(usbd_dev, MSC_SENDING_EP, USB_ENDPOINT_ATTR_BULK, MSC_ENDPOINT_PACKAGE_SIZE, NULL);

    tx_ep_sizes[0] = USBMANAGER_FIFO0_SIZE;
    tx_ep_sizes[CDC_SENDING_EP & 0x7F] = CDC_ENDPOINT_PACKAGE_SIZE;
    tx_ep_sizes[CDC_INTERRUPT_EP & 0x7F] = 16;
    tx_ep_sizes[MSC_SENDING_EP & 0x7F] = MSC_ENDPOINT_PACKAGE_SIZE;

    usbd_register_control_callback(
            usbd_dev,
            USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
            USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
            usbmanager_control_request);
}

void usbmanager_init(void) {
    usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config, usb_strings, 3, (uint8_t*) controlBuffer, 128);
    usbd_register_set_config_callback(usbd_dev, usbmanager_set_config);
}

void usbmanager_poll(void) {
    usbd_poll(usbd_dev);
    msc_stateMachine(usbd_dev);
}

u16 usbmanager_send_packet(u8 addr, const void *buf, u16 len) {
    if (tx_ep_sizes[addr & 0x7F] < len) {
        len = tx_ep_sizes[addr & 0x7F];
    }
    return usbd_ep_write_packet(usbd_dev, addr, buf, len);
}
