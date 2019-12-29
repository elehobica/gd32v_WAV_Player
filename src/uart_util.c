#include "uart_util.h"

void init_uart0(void)
{	
	/* enable GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOA);
    /* enable USART clock */
    rcu_periph_clock_enable(RCU_USART0);

    /* connect port to USARTx_Tx */
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    /* connect port to USARTx_Rx */
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_10);

	/* USART configure */
    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART0, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART0, USART_CTS_DISABLE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

    usart_interrupt_enable(USART0, USART_INT_RBNE);
}

static void dump_reg(uint32_t addr, uint32_t size)
{
    addr = (addr / 0x10) * 0x10;
    uint32_t i = 0;
    while (i < size) {
        printf("%08x:", (int) (addr + i));
        printf(" %08x", (int) REG32(addr+i+0x0));
        printf(" %08x", (int) REG32(addr+i+0x4));
        printf(" %08x", (int) REG32(addr+i+0x8));
        printf(" %08x", (int) REG32(addr+i+0xc));
        printf("\n");
        i += 0x10;
    }
}

void dump_reg_all(void)
{
    printf("\n");
    printf("[RCU]");
    printf("\n");
    dump_reg(RCU_BASE, 0x40);
    printf("[AFIO]");
    printf("\n");
    dump_reg(AFIO_BASE, 0x20);
    printf("[GPIOA]");
    printf("\n");
    dump_reg(GPIO_BASE, 0x20);
    printf("[GPIOB]");
    printf("\n");
    dump_reg(GPIO_BASE+0x400, 0x20);
    /*
    printf("[SPI1]");
    printf("\n");
    dump_reg(SPI1, 0x30);
    */
    printf("[SPI2]");
    printf("\n");
    dump_reg(SPI2, 0x30);
    printf("[DMA1]");
    printf("\n");
    dump_reg(DMA1, 0x200);
}

int _put_char(int ch)
{
    usart_data_transmit(USART0, (uint8_t) ch );
    while ( usart_flag_get(USART0, USART_FLAG_TBE)== RESET){
    }

    return ch;
}
