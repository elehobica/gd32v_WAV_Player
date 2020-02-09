#include "gd32vf103_adc.h"
#include "gd32vf103_dma.h"
#include "adc_util.h"

dma_parameter_struct dma_adc0;
volatile uint16_t adc0_rdata;

void adc_init(void)
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
    adc_regular_channel_config(ADC0, 0, ADC_CHANNEL_0, ADC_SAMPLETIME_7POINT5);
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

uint32_t adc_get_hp_button(void)
{
    uint32_t ret;
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
    return ret;
}