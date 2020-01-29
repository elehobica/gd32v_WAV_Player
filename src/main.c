#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include <string.h>
#include "uart_util.h"
#include "audio_buf.h"

unsigned char image[12800];

//#define SIZE_OF_SAMPLES 512  // samples for 2ch

int main(void)
{
    //uint8_t mount_is_ok = 1; /* 0: mount successful ; 1: mount failed */

    int count = 0;
    FATFS fs;
    int offset = 0;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

    init_uart0();

    Lcd_Init();			// init OLED
    LCD_Clear(WHITE);
    BACK_COLOR=WHITE;

    LEDR(1);
    LEDG(1);
    LEDB(1);

    // Mount FAT
    fr = f_mount(&fs, "", 1);
    while (fr == 1) { 
        delay_1ms(10);
        fr = f_mount(&fs, "", 1);
        if (count++ > 100) break;
    }

    if (fr == 0) {
        // Opening Logo
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
        //delay_1ms(1500);
        f_close(&fil);

        // Start Audio
        while (1) {
            prepare_audio_buf();
            while (run_audio_buf()) {
                LEDB_TOG;
            }
        }
    } else {
        LCD_ShowString(24,  0, (u8 *)("no card found!"), BLACK);
        LCD_ShowString(24, 16, (u8 *)("no card found!"), BLUE);
        LCD_ShowString(24, 32, (u8 *)("no card found!"), BRED);
        LCD_ShowString(24, 48, (u8 *)("no card found!"), GBLUE);
        LCD_ShowString(24, 64, (u8 *)("no card found!"), RED);
        while (1) {
            LEDR_TOG;
            delay_1ms(200);
            LEDG_TOG;
            delay_1ms(200);
            LEDB_TOG;
            delay_1ms(200);
        }
    }
}

