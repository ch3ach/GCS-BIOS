/*
 * dac_ltc2632.c
 *
 *  Created on: 25 Oct 2013
 *      Author: vlaad
 */

#include <libopencm3/stm32/f4/spi.h>
#include "dac_ltc2632.h"

#define DAC_COMMAND_WRITE_UPDATE_N 0x3

void set_dac_value(int dac_select, uint8_t value)
{
	spi_send(SPI1, (DAC_COMMAND_WRITE_UPDATE_N << 4) | dac_select);
	spi_send(SPI1, value);
}
