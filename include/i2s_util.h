#ifndef _I2S_UTIL_H_
#define _I2S_UTIL_H_

#include "gd32vf103_gpio.h"
#include "gd32vf103_spi.h"

void init_i2s1(void);
void init_i2s2(void);
void init_dma_i2s2(uint32_t memory_addr, uint32_t trans_number);

#endif
