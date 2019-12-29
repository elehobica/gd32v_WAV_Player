#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include <string.h>
#include "uart_util.h"
#include "i2s_util.h"

unsigned char image[12800];
FATFS fs;

int main(void)
{
    uint8_t mount_is_ok = 1; /* 0: mount successful ; 1: mount failed */
    int offset = 0;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

    init_uart0();
    init_i2s2();

    dump_reg_all();

    while (1) {
        spi_i2s_data_transmit(SPI2, 0xaaaa);
        spi_i2s_data_transmit(SPI2, 0xaaaa);
        spi_i2s_data_transmit(SPI2, 0x5555);
        spi_i2s_data_transmit(SPI2, 0x5555);
        /*
        PA_OUT(15, 1);
        PB_OUT(3, 1);
        PB_OUT(5, 1);
        */
        LEDB_TOG;
        delay_1ms(1);
        /*
        PA_OUT(15, 0);
        PB_OUT(3, 0);
        PB_OUT(5, 0);
        delay_1ms(1);
        */
    }

    Lcd_Init();			// init OLED
    LCD_Clear(WHITE);
    BACK_COLOR=WHITE;

    LEDR(1);
    LEDG(1);
    LEDB(1);



    fr = f_mount(&fs, "", 1);
    if (fr == 0)
        mount_is_ok = 0;
    else
        mount_is_ok = 1;

    if (mount_is_ok == 0)
    {

        while(1)
        {
            offset = 0;
            fr = f_open(&fil, "logo.bin", FA_READ);
            if (fr) printf("open error: %d!\n\r", (int)fr);
            f_lseek(&fil, offset);
            fr = f_read(&fil, image, sizeof(image), &br);
            LCD_ShowPicture(0,0,159,39);
            offset += 12800;
            LEDB_TOG;
            f_lseek(&fil, offset);
            fr = f_read(&fil, image, sizeof(image), &br);
            LCD_ShowPicture(0,40,159,79);
            LEDB_TOG;
            delay_1ms(1500);
            f_close(&fil);

            fr = f_open(&fil, "bmp.bin", FA_READ);
            if (fr) printf("open error: %d!\n\r", (int)fr);
            offset = 0;

            for (int i=0; i<2189;i++)
            {
                fr = f_read(&fil, image, sizeof(image), &br);
                LCD_ShowPicture(0,0,159,39);
                offset += 12800;
                f_lseek(&fil, offset);
                LEDB_TOG;
                //delay_1ms(500);
                fr = f_read(&fil, image, sizeof(image), &br);
                LCD_ShowPicture(0,40,159,79);
                offset += 12800;
                f_lseek(&fil, offset);
                LEDB_TOG;
                //delay_1ms(500);
                //printf("loop = %d\n", i);
            }

            /* Close the file */
            f_close(&fil);
        }
    }
    else
    {
        LCD_ShowString(24,  0, (u8 *)("no card found!"), BLACK);
        LCD_ShowString(24, 16, (u8 *)("no card found!"), BLUE);
        LCD_ShowString(24, 32, (u8 *)("no card found!"), BRED);
        LCD_ShowString(24, 48, (u8 *)("no card found!"), GBLUE);
        LCD_ShowString(24, 64, (u8 *)("no card found!"), RED);
        while (1)
        {
            LEDR_TOG;
            delay_1ms(200);
            LEDG_TOG;
            delay_1ms(200);
            LEDB_TOG;
            delay_1ms(200);
        }
    }
}

