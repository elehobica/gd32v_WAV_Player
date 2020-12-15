/*------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "gd32vf103_adc.h"
#include "gd32vf103_dma.h"
#include "adc_util.h"

dma_parameter_struct dma_adc0;
volatile uint16_t adc0_rdata;

void adc0_init(void)
{
    // use PA0 pin
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_0);

    // ADC clock (max 14MHz): 108MHz / 8 = 13.5MHz
    rcu_adc_clock_config(RCU_CKADC_CKAPB2_DIV8); 

    rcu_periph_clock_enable(RCU_ADC0);
    // Continuous conversion mode with DMA
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_dma_mode_enable(ADC0);
    // set order rank0 = PA0, sample time = 7.5 cycle @13.5MHz 
    // convert time = 7.5 + 12.5 = 20 cycle @13.5MHz = 1.48us
    //adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_7POINT5);
    // Too short sample time causes input impedance low (1.46kohm@1.5cyc, 9.49kohm@7.5cyc)
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_55POINT5);
    adc_channel_length_config(ADC0, ADC_REGULAR_CHANNEL, 1);
    // set oversampling-average mode
    adc_oversample_mode_config(ADC0, ADC_OVERSAMPLING_ALL_CONVERT, ADC_OVERSAMPLING_SHIFT_NONE, ADC_OVERSAMPLING_RATIO_MUL8);

    // DMA setting
    rcu_periph_clock_enable(RCU_DMA0);

    dma_struct_para_init(&dma_adc0);
    dma_adc0.periph_addr = (uint32_t) &ADC_RDATA(ADC0);
    dma_adc0.periph_width = DMA_PERIPHERAL_WIDTH_16BIT;
    dma_adc0.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_adc0.memory_addr = (uint32_t) &adc0_rdata;
    dma_adc0.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_adc0.memory_inc = DMA_MEMORY_INCREASE_DISABLE;
    dma_adc0.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_adc0.number = 1;
    dma_adc0.priority = DMA_PRIORITY_LOW;
    dma_init(DMA0, DMA_CH0, &dma_adc0);
    dma_circulation_enable(DMA0, DMA_CH0);
    dma_channel_enable(DMA0, DMA_CH0);

    // ADC0 Enable
    adc_enable(ADC0);

    // Trigger (only first time thanks to Continuous conversion mode)
    adc_external_trigger_source_config(ADC0, ADC_REGULAR_CHANNEL, ADC0_1_EXTTRIG_REGULAR_NONE); // set source to SW trigger
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
}

uint8_t adc0_get_hp_button(void)
{
    uint8_t ret;
    // 3.3V ~ 5.0V both support (high mistake rate)
    /*
    if (adc0_rdata < 198) { // < 160mV  4095*160/3300 (CENTER)
        ret = HP_BUTTON_CENTER;
    } else if (adc0_rdata >= 223 && adc0_rdata < 384) { // 180mv ~ 310mV (D)
        ret = HP_BUTTON_D;
    } else if (adc0_rdata >= 384 && adc0_rdata < 645) { // 310mV ~ 520mV (PLUS)
        ret = HP_BUTTON_PLUS;
    } else if (adc0_rdata >= 670 && adc0_rdata < 1116) { // 540mV ~ 900mV (MINUS)
        ret = HP_BUTTON_MINUS;
    } else { // others
        ret = HP_BUTTON_OPEN;
    }
    */
    // 3.3V support
    if (adc0_rdata < 4095*100/3300) { // < 100mV  4095*100/3300 (CENTER)
        ret = HP_BUTTON_CENTER;
    } else if (adc0_rdata >= 4095*142/3300 && adc0_rdata < 4095*238/3300) { // 142mv ~ 238mV (D: 190mV)
        ret = HP_BUTTON_D;
    } else if (adc0_rdata >= 4095*240/3300 && adc0_rdata < 4095*400/3300) { // 240mV ~ 400mV (PLUS: 320mV)
        ret = HP_BUTTON_PLUS;
    } else if (adc0_rdata >= 4095*435/3300 && adc0_rdata < 4095*725/3300) { // 435mV ~ 725mV (MINUS: 580mV)
        ret = HP_BUTTON_MINUS;
    } else { // others
        ret = HP_BUTTON_OPEN;
    }
    return ret;
}

uint16_t adc0_get_raw_data(void)
{
    return adc0_rdata;
}

void adc1_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_AIN, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    adc_calibration_enable(ADC1);

    RCU_CFG0 |= (0b10 << 14) | (1 << 28); // ADC clock = 108MHz / 8 = 13.5MHz(14MHz max.)
    rcu_periph_clock_enable(RCU_ADC1);

    adc_enable(ADC1);
}

uint16_t adc1_get(int ch)
{
    ADC_RSQ2(ADC1) = ch;
    //ADC_SAMPT1(ADC1) = 0x3;

    ADC_SAMPT1(ADC1) = ADC_SAMPLETIME_239POINT5;
    ADC_CTL1(ADC1) |= ADC_CTL1_ADCON;
    /*
    ADC_CTL1(ADC1) |= ADC_CTL1_ADCON | (0x7<<17);
    ADC_CTL1(ADC1) |= ADC_CTL1_SWRCST;
    */

    while( !(ADC_STAT(ADC1) & ADC_STAT_EOC) );

    uint16_t ret = ADC_RDATA(ADC1) & 0xFFFF;
    ADC_STAT(ADC1) &= ~ADC_STAT_EOC;
    return ret;
}

uint16_t adc1_get_bat_x100(void) // outputs 0 ~ 99
{
    int i;
    uint32_t adc_val = 0;

    for (i = 0; i < 5; i++) {
        adc_val += adc1_get(3);
    }
    adc_val /= 5;
    // assuming register ratio 1kohm : 3.3kohm
    // voltage ratio = 33 / (10 + 33)
    adc_val = (adc_val * 3300 * (10+33) / 33 / 4095);
    //printf("Voltage: %d mV\n\r", adc_val);
    if (adc_val < 2900) {
        adc_val = 0;
    } else if (adc_val > 4100) {
        adc_val = 99;
    } else {
        adc_val -= 2900;
        adc_val = adc_val * 100 / (4100-2900);
    }
    return adc_val;
}
