#include "i2s_util.h"

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

    //spi_i2s_interrupt_enable(SPI2, SPI_I2S_INT_TBE);
}
