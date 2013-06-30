/* 
 * File:   led.h
 * Author: gandhi
 *
 * Created on 22. Juni 2013, 15:52
 */

#ifndef LED_H
#define	LED_H

#include <libopencm3/stm32/f4/gpio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define led_init()      gpio_mode_setup(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, \
                                        GPIO12 | GPIO13 | GPIO14 | GPIO15); \
                        gpio_clear(GPIOD, GPIO12 | GPIO13 | GPIO14 | GPIO15);
    
#define led_red_on()     gpio_set(GPIOD, GPIO14);
#define led_red_off()    gpio_clear(GPIOD, GPIO14);
#define led_red_toggle() gpio_toggle(GPIOD, GPIO14);
    
#define led_green_on()     gpio_set(GPIOD, GPIO12);
#define led_green_off()    gpio_clear(GPIOD, GPIO12);
#define led_green_toggle() gpio_toggle(GPIOD, GPIO12);
    
#define led_blue_on()     gpio_set(GPIOD, GPIO15);
#define led_blue_off()    gpio_clear(GPIOD, GPIO15);
#define led_blue_toggle() gpio_toggle(GPIOD, GPIO15);
    
#define led_orange_on()     gpio_set(GPIOD, GPIO13);
#define led_orange_off()    gpio_clear(GPIOD, GPIO13);
#define led_orange_toggle() gpio_toggle(GPIOD, GPIO13);


#ifdef	__cplusplus
}
#endif

#endif	/* LED_H */

