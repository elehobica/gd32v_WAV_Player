#ifndef _ADC_UTIL_H_
#define _ADC_UTIL_H_

#define HP_BUTTON_OPEN     0
#define HP_BUTTON_CENTER   1
#define HP_BUTTON_D        2
#define HP_BUTTON_PLUS     3
#define HP_BUTTON_MINUS    4

void adc0_init(void);
uint8_t adc0_get_hp_button(void);
uint16_t adc0_get_raw_data(void);

void adc1_init(void);
uint16_t adc1_get(int ch);
uint16_t adc1_get_bat_x100(void);

#endif
