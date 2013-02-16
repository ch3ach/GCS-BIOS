#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include "messagehistory.h"
#include "usbmanager.h"
#include "usbstorage.h"
#include "cdcacm.h"


static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_CLASS_MISCELLANEOUS,
    .bDeviceSubClass = 0x02,
    .bDeviceProtocol = 0x01,
    .bMaxPacketSize0 = 64,
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
    }
};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 2,
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

static int cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, u8 **buf,
        u16 *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req)) {
    (void) complete;
    (void) buf;
    (void) usbd_dev;

    gpio_set(GPIOD, GPIO13);
    gpio_toggle(GPIOD, GPIO14);

    history_usbControl(req, sizeof (struct usb_setup_data), *buf, *len);

    switch (req->bRequest) {
        case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
        {
            /*
             * This Linux cdc_acm driver requires this to be implemented
             * even though it's optional in the CDC spec, and we don't
             * advertise it in the ACM functional descriptor.
             */
            return 1;
        }
        case USB_CDC_REQ_SET_LINE_CODING:
            if (*len < sizeof (struct usb_cdc_line_coding))
                return 0;

            return 1;
    }
    return 0;
}

static void cdcacm_set_config(usbd_device *usbd_dev, u16 wValue) {
    (void) wValue;

    usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
    usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL);
    usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

    usbd_register_control_callback(
            usbd_dev,
            USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
            USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
            cdcacm_control_request);
}

void usbmanager_init(void) {
    usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config, usb_strings, 3);
    usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);
}

void usbmanager_poll(void) {
    usbd_poll(usbd_dev);
}

u16 usbmanager_send_packet(u8 addr, const void *buf, u16 len) {
    return usbd_ep_write_packet(usbd_dev, addr, buf, len);
}
