/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include "messagehistory.h"
#include "cdcacm.h"

static const struct usb_endpoint_descriptor data_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x01,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = CDC_ENDPOINT_PACKAGE_SIZE,
        .bInterval = 1,
    },
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x81,
        .bmAttributes = USB_ENDPOINT_ATTR_BULK,
        .wMaxPacketSize = CDC_ENDPOINT_PACKAGE_SIZE,
        .bInterval = 1,
    }
};


/*
 * This notification endpoint isn't implemented. According to CDC spec it's
 * optional, but its absence causes a NULL pointer dereference in the
 * Linux cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {
    {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = 0x82,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = 16,
        .bInterval = 255,
    }
};

static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call_mgmt;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
    .header =
    {
        .bFunctionLength = sizeof (struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC = 0x0110,
    },
    .call_mgmt =
    {
        .bFunctionLength =
        sizeof (struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = 1,
    },
    .acm =
    {
        .bFunctionLength = sizeof (struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities = 0,
    },
    .cdc_union =
    {
        .bFunctionLength = sizeof (struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bControlInterface = 0,
        .bSubordinateInterface0 = 1,
    }
};

const struct usb_interface_descriptor comm_iface[] = {
    {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 1,
        .bInterfaceClass = USB_CLASS_CDC,
        .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
        .bInterfaceProtocol = USB_CDC_PROTOCOL_NONE,
        .iInterface = 0,

        .endpoint = comm_endp,

        .extra = &cdcacm_functional_descriptors,
        .extralen = sizeof (cdcacm_functional_descriptors)
    }
};

const struct usb_interface_descriptor data_iface[] = {
    {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 1,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_DATA,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,

        .endpoint = data_endp,
    }
};

char recvBuf[USB_CDC_RECV_BUFFER_COUNT][USB_CDC_RECV_BUFFER_SIZE];
volatile int32_t recvBufLen[USB_CDC_RECV_BUFFER_COUNT];
volatile int32_t readBuffer = 1;

volatile int32_t writeOffset = 0;
volatile int32_t writeBuffer = 0;

void cdcacm_data_rx_cb(usbd_device *usbd_dev, u8 ep) {
    int32_t n, n_old = 0;

    char buf[CDC_ENDPOINT_PACKAGE_SIZE];
    int len = usbd_ep_read_packet(usbd_dev, ep, buf, CDC_ENDPOINT_PACKAGE_SIZE);

    for (n = 0; n < len; n++) {
        if ((buf[n] == '\n') || ((writeOffset + n - n_old) >= USB_CDC_RECV_BUFFER_SIZE - 1)) {
            recvBuf[writeBuffer][writeOffset + n - n_old] = 0;
            recvBufLen[writeBuffer] = writeOffset + n - n_old;
            writeOffset = 0;
            readBuffer = writeBuffer;
            if (writeBuffer == 0)
                writeBuffer = 1;
            else
                writeBuffer = 0;
            n_old = n + 1;
        } else {
            recvBuf[writeBuffer][writeOffset + n - n_old] = buf[n];
        }
    }
    writeOffset += n - n_old;

    gpio_toggle(GPIOD, GPIO12);
}
