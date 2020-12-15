/*------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#ifndef _TIMER_PWM_H_
#define _TIMER_PWM_H_

#include <stdint.h>

void timer0_irq_init(void);
void timer0_irq_stop(void);
void timer1_pwm_init(void);
void timer1_pwm_set_ratio(uint16_t on_term);
void timer3_pwm_init(void);
void timer3_pwm_set_ratio(uint16_t on_term);

#endif
