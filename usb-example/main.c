

#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/cm3/scb.h>
#include "messagehistory.h"
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

typedef enum {
    INTERPRETER_WAIT,
    INTERPRETER_SENDHIST
} interpreter_state_t;

uint32_t histbuffer[512];

int main(void) {
    usbd_device *usbd_dev;
    interpreter_state_t state = INTERPRETER_WAIT;
    char strBuf[64];
    uint16_t strLen = 0;

    rcc_clock_setup_hse_3v3(&hse_8mhz_3v3[CLOCK_3V3_168MHZ]);

    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPCEN);
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);
    rcc_peripheral_enable_clock(&RCC_AHB2ENR, RCC_AHB2ENR_OTGFSEN);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
            GPIO9 | GPIO11 | GPIO12);
    gpio_set_af(GPIOA, GPIO_AF10, GPIO9 | GPIO11 | GPIO12);

    gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
            GPIO12 | GPIO13 | GPIO14 | GPIO15);
    gpio_clear(GPIOD, GPIO12 | GPIO13 | GPIO14 | GPIO15);

    gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0);
    gpio_clear(GPIOC, GPIO0);

    recvBufLen[0] = 0;
    recvBufLen[1] = 0;

    history_init((uint8_t*) histbuffer, sizeof (histbuffer));

    usbd_dev = usbd_init(&otgfs_usb_driver, &dev, &config, usb_strings, 3);
    usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);

    while (1) {
        usbd_poll(usbd_dev);
        switch (state) {
            case INTERPRETER_WAIT:
                if (recvBufLen[readBuffer] != 0) {
                    if (memcmp(recvBuf[readBuffer], "get", 3) == 0) {
                        state = INTERPRETER_SENDHIST;
                    } else {
                        usbd_ep_write_packet(usbd_dev, 0x82, "STM32: unknown command\n", 23);
                    }
                    recvBufLen[readBuffer] = 0;
                }
                break;
            case INTERPRETER_SENDHIST:
                if (strLen == 0) {
                    strLen = history_getASCIIPackage(strBuf, sizeof (strBuf));
                }
                if (strLen == 0) {
                    state = INTERPRETER_WAIT;
                } else {
                    if (usbd_ep_write_packet(usbd_dev, 0x82, strBuf, strLen) != 0)
                        strLen = 0;
                }
                break;
        }
    }
}
