#include "i2s_util.h"
#include "gd32vf103_dma.h"

dma_parameter_struct dma_param;

void init_i2s1(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_SPI1);

    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15);

    i2s_init(SPI1, I2S_MODE_MASTERTX, I2S_STD_PHILLIPS, I2S_CKPL_HIGH);
    i2s_psc_config(SPI1, I2S_AUDIOSAMPLE_44K, I2S_FRAMEFORMAT_DT24B_CH32B, I2S_MCKOUT_DISABLE);
    i2s_enable(SPI1);

    //spi_i2s_interrupt_enable(SPI1, SPI_I2S_INT_TBE);
}

void init_dma_i2s2(uint32_t memory_addr, uint32_t trans_number)
{
    rcu_periph_clock_enable(RCU_DMA1);

    dma_struct_para_init(&dma_param);
    dma_param.periph_addr = &SPI_DATA(SPI2);
    dma_param.periph_width = DMA_PERIPHERAL_WIDTH_32BIT;
    dma_param.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_param.memory_addr = memory_addr;
    dma_param.memory_width = DMA_MEMORY_WIDTH_16BIT;
    dma_param.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_param.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_param.number = trans_number;
    dma_param.priority = DMA_PRIORITY_HIGH;
    dma_init(DMA1, DMA_CH1, &dma_param);
}

void init_i2s2(void)
{
    rcu_periph_clock_enable(RCU_AF);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_SPI2);

    gpio_pin_remap_config(GPIO_SWJ_DISABLE_REMAP, ENABLE);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_15);
    gpio_init(GPIOB, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3 | GPIO_PIN_5);    

    i2s_init(SPI2, I2S_MODE_MASTERTX, I2S_STD_PHILLIPS, I2S_CKPL_HIGH);
    i2s_psc_config(SPI2, I2S_AUDIOSAMPLE_44K, I2S_FRAMEFORMAT_DT24B_CH32B, I2S_MCKOUT_DISABLE);
    i2s_enable(SPI2);
}
