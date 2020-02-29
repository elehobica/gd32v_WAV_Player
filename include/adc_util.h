#ifndef _ADC_UTIL_H_
#define _ADC_UTIL_H_

#define HP_BUTTON_OPEN     0
#define HP_BUTTON_CENTER   1
#define HP_BUTTON_D        2
#define HP_BUTTON_PLUS     3
#define HP_BUTTON_MINUS    4

void adc_init(void);
uint32_t adc_get_hp_button(void);
uint16_t adc_get_raw_data(void);

#endif
