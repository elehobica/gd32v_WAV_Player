#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include <string.h>
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"
#include "fifo.h"
#include "cfifo.h"

unsigned char image[12800];
extern int volume;
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
}

uint32_t button_prv = HP_BUTTON_OPEN;
uint32_t button_repeat_count = 0;

void tick_100ms(void)
{
    uint32_t button = adc_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
    } else if (button != button_prv || button_repeat_count > 20) {
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (volume < 100) volume++;
        } else if (button == HP_BUTTON_MINUS) {
            if (volume > 0) volume--;
        }
    } else {
        button_repeat_count++;
    }
    button_prv = button;
}

void tick_1sec(void)
{
    LEDR_TOG;
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
    //delay_1ms(1500);
    f_close(&fil);

    cfifo_t *cfifo = cfifo_create(5);
    // Start Audio
    count = 0;
    while (1) {
        cfifo_write(cfifo, "play1.wav");
        cfifo_write(cfifo, "play2.wav");
        cfifo_write(cfifo, "play3.wav");
        while (!cfifo_is_empty(cfifo)) {
            cfifo_data_t wav_filename;
            cfifo_read(cfifo, wav_filename);
            printf("Play \"%s\"\n\r", (char *) wav_filename);
            prepare_audio_buf(wav_filename);
            while (run_audio_buf()) {
            }
        }
    }
}





