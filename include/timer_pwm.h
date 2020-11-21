#ifndef _TIMER_PWM_H_
#define _TIMER_PWM_H_

void timer0_irq_init(void);
void timer3_pwm_init(void);
void timer3_pwm_set_ratio(uint16_t on_term);

#endif
