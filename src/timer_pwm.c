#include "gd32vf103_timer.h"
#include "gd32vf103_gpio.h"
#include "timer_pwm.h"

void timer0_irq_init(void)
{
    timer_parameter_struct tpa;

    rcu_periph_clock_enable(RCU_TIMER0);
    timer_struct_para_init(&tpa);
    tpa.prescaler = 1080 - 1;       // prescaler (108MHz -> 100KHz)
    tpa.period = 100 - 1;           // max value of counting up (100KHz -> 1000Hz = 1ms)
    tpa.repetitioncounter = 10 - 1; // the num of overflows that issues update IRQ. (1ms*10 = 10ms)
    timer_init(TIMER0, &tpa);
    timer_interrupt_enable(TIMER0, TIMER_INT_UP);
    timer_enable(TIMER0);

    eclic_global_interrupt_enable();
    eclic_enable_interrupt(TIMER0_UP_IRQn);
}

void timer3_pwm_init(void)
{
    timer_parameter_struct tpa;

    rcu_periph_clock_enable(RCU_TIMER3);
    timer_struct_para_init(&tpa);
    tpa.prescaler = 108 - 1;        // prescaler (108MHz -> 1MHz)
    tpa.period = 100 - 1;           // max value of counting up (1MHz -> 10KHz = 100us)
    timer_init(TIMER3, &tpa);
    timer_channel_output_state_config(TIMER3, TIMER_CH_1, TIMER_CCX_ENABLE); // channel output enable
    timer_channel_output_mode_config(TIMER3, TIMER_CH_1, TIMER_OC_MODE_PWM0);
    // default PWM ratio = 50 / 100
    timer_autoreload_value_config(TIMER3, 100);
    timer_channel_output_pulse_value_config(TIMER3, TIMER_CH_1, 50);
    timer_enable(TIMER3);

    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
}

void timer3_pwm_set_ratio(uint16_t on_term)
{
    timer_channel_output_pulse_value_config(TIMER3, TIMER_CH_1, on_term);
}