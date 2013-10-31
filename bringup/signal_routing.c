/*
 * signal_routing.c
 *
 *  Created on: 17 Oct 2013
 *      Author: vlaad
 */

#include <libopencm3/stm32/gpio.h>
#include "signal_routing.h"

#define GPIO_MASK_ANT_BYPASS GPIO4
#define GPIO_MASK_RAMP_BYPASS GPIO14
#define GPIO_MASK_MIXER_RX GPIO8
#define GPIO_MASK_MIXER_TX GPIO9

void rx_mixer_oe(int enable)
{
	if (enable)
		gpio_clear(GPIOB, GPIO_MASK_MIXER_RX);
	else
		gpio_set(GPIOB, GPIO_MASK_MIXER_RX);
}

void tx_mixer_oe(int enable)
{
	if (enable)
		gpio_clear(GPIOB, GPIO_MASK_MIXER_TX);
	else
		gpio_set(GPIOB, GPIO_MASK_MIXER_TX);
}

void ant_to_bypass(int enable)
{
	if (enable)
		gpio_set(GPIOB, GPIO_MASK_ANT_BYPASS);
	else
		gpio_clear(GPIOB, GPIO_MASK_ANT_BYPASS);
}

void ramp_to_bypass(int enable)
{
	if (enable)
		gpio_set(GPIOB, GPIO_MASK_RAMP_BYPASS);
	else
		gpio_clear(GPIOB, GPIO_MASK_RAMP_BYPASS);
}

void signal_routing(int mode)
{
	switch(mode)
	{
	case SIGNAL_ROUTING_RX_NOLNA:
		ant_to_bypass(1);
		ramp_to_bypass(1);
		tx_mixer_oe(0);
		break;
	case SIGNAL_ROUTING_RX_LNA:
		ant_to_bypass(0);
		ramp_to_bypass(0);
		tx_mixer_oe(0);
		break;
	case SIGNAL_ROUTING_TX:
		ant_to_bypass(1);
		ramp_to_bypass(1);
		tx_mixer_oe(1);
		break;
	}
}
