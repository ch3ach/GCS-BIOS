

#include <stdlib.h>
#include <libopencm3/stm32/f4/rcc.h>
#include <libopencm3/stm32/f4/gpio.h>
#include "SCSIUnknowns.h"
#include "cdcacm.h"
#include "usbmanager.h"
#include "led.h"
#include "ramdisk.h"

typedef enum {
    INTERPRETER_WAIT,
    INTERPRETER_SENDHIST
} interpreter_state_t;

uint32_t SCSIUnknownsBuffer[512];

int main(void) {
    interpreter_state_t state = INTERPRETER_WAIT;
    char strBuf[64];
    uint16_t strLen = 0;
    uint32_t n;

    rcc_clock_setup_hse_3v3(&hse_8mhz_3v3[CLOCK_3V3_168MHZ]);

    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPAEN);
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPCEN);
    rcc_peripheral_enable_clock(&RCC_AHB1ENR, RCC_AHB1ENR_IOPDEN);
    rcc_peripheral_enable_clock(&RCC_AHB2ENR, RCC_AHB2ENR_OTGFSEN);

    gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE,
            GPIO9 | GPIO11 | GPIO12);
    gpio_set_af(GPIOA, GPIO_AF10, GPIO9 | GPIO11 | GPIO12);

    led_init();

    recvBufLen[0] = 0;
    recvBufLen[1] = 0;

    SCSIUnknowns_init(SCSIUnknownsBuffer, sizeof (SCSIUnknownsBuffer));

    usbmanager_init();

    for (n = 0; n < ramdisk_getSize(); n += 16) {
        ramdisk_write(n, "0123456789abcde\n", 16);
    }
    ramdisk_write(ramdisk_getSize() - 16, "<- END\n 128 kB\n", 16);

    while (1) {
        usbmanager_poll();
        switch (state) {
            case INTERPRETER_WAIT:
                if (recvBufLen[readBuffer] != 0) {
                    if (memcmp(recvBuf[readBuffer], "get", 3) == 0) {
                        state = INTERPRETER_SENDHIST;
                        SCSIUnknowns_restartASCII();
                    } else {
                        usbmanager_send_packet(CDC_SENDING_EP, "STM32: unknown command\n", 23);
                    }
                    recvBufLen[readBuffer] = 0;
                }
                break;
            case INTERPRETER_SENDHIST:
                if (strLen == 0) {
                    strLen = SCSIUnknowns_nextASCII(strBuf, sizeof (strBuf));
                }
                if (strLen == 0) {
                    state = INTERPRETER_WAIT;
                    SCSIUnknowns_disposeData();
                } else {
                    if (usbmanager_send_packet(CDC_SENDING_EP, strBuf, strLen) != 0)
                        strLen = 0;
                }
                break;
        }
    }
}
