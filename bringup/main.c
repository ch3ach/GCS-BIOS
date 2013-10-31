/*
 * main.c
 *
 *  Created on: 19 Oct 2013
 *      Author: vlaad
 */

#include <libopencm3/stm32/gpio.h>
#include <signal_routing.h>

void configure_pinmux()
{
	// see st32f40x datasheet p58ff
	gpio_set_af(GPIOA, GPIO_AF0, GPIO0);
}

int main(void)
{
	signal_routing(SIGNAL_ROUTING_RX_NOLNA);
	return 0;
}
