/*
 * dac_ltc2632.h
 *
 *  Created on: 16 Oct 2013
 *      Author: vlaad
 */

#ifndef DAC_LTC2632_H_
#define DAC_LTC2632_H_

#include <stdint.h>

#define SELECT_DAC_A 0x01
#define SELECT_DAC_B 0x02
#define SELECT_BOTH_DACS 0x0F

void set_dac_value(int dac_select, uint8_t value);

#endif /* DAC_LTC2632_H_ */
