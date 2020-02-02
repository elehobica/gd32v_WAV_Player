#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include <string.h>
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"

unsigned char image[12800];
int count10ms = 0;

void timer_irq_init(void)
{
    rcu_periph_clock_enable(RCU_TIMER0);
    timer_parameter_struct tpa;
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

void tick_10ms(void)
{
    LEDG(1);
}

uint32_t button_prv = HP_BUTTON_OPEN;
uint32_t button_repeat_count = 0;

void tick_100ms(void)
{
    uint32_t button = adc_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
    } else if (button != button_prv) {
        if (button == HP_BUTTON_CENTER) {
            audio_pause();
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            volume_up();
        } else if (button == HP_BUTTON_MINUS) {
            volume_down();
        }
    } else if (button_repeat_count > 20) {
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            volume_up();
        } else if (button == HP_BUTTON_MINUS) {
            volume_down();
        }
    } else if (button == button_prv) {
        button_repeat_count++;
    }
    button_prv = button;
}

void tick_1sec(void)
{
    //LEDG_TOG;
    LEDG(0);
}

void TIMER0_UP_IRQHandler(void)
{
    tick_10ms();
    if (count10ms % 10 == 0) {
        tick_100ms();
    }
    if (count10ms++ >= 100) {
        count10ms = 0;
        tick_1sec();
    }
    timer_interrupt_flag_clear(TIMER0, TIMER_INT_FLAG_UP);
}

int main(void)
{
    int count = 0;
    FATFS fs;
    int offset = 0;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    char lcd_str[256];

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

    init_uart0();
    adc_init();
    timer_irq_init();

    // init OLED
    Lcd_Init();
    LCD_Clear(WHITE);
    BACK_COLOR=WHITE;

    // LED All Off
    LEDR(1);
    LEDG(1);
    LEDB(1);

    // Mount FAT
    fr = f_mount(&fs, "", 1); // 0: mount successful ; 1: mount failed
    while (fr == 1) { 
        delay_1ms(10);
        fr = f_mount(&fs, "", 1);
        if (count++ > 100) break;
    }

    if (fr) { // Mount Fail (Loop)
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
            eclic_system_reset();
        }
    }

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
    delay_1ms(500);
    f_close(&fil);

    // Audio play
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    audio_init();
    while (1) {
        if (!audio_is_playing()) {
            audio_add_playlist_wav("play1.wav");
            audio_add_playlist_wav("play2.wav");
            audio_add_playlist_wav("play3.wav");
            audio_play();
        }
        const audio_info_type *audio_info = audio_get_info();
        sprintf(lcd_str, "VOL %3d", volume_get());
        LCD_ShowString(8*0,  16*0, (u8 *) audio_info->filename, GBLUE);
        LCD_ShowString(8*12, 16*4, (u8 *) lcd_str, WHITE);

        //printf("%s %d/%d\n\r", audio_info.filename, audio_info.offset, audio_info.size);
        delay_1ms(100);
    }
}





