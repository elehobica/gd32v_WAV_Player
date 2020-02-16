#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include "fatfs/ff_util.h"
#include <string.h>
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"
#include "fifo/stack.h"

unsigned char image[160*80*2/2];
int count10ms = 0;

enum mode_enm {
    FileView = 0,
    Play
};

enum mode_enm mode = FileView;
int idx_req = 1;
int idx_req_open = 0;
int aud_req = 0;

int idx_head = 0;
int idx_column = 0;
int idx_idle_count = 0;
int idx_play = 0;
int idx_play_count = 0;
int cover_exists = 0;
char file_str[FF_LFN_BUF+1];

void idx_open(void)
{
    if (idx_req_open != 1) {
        idx_req_open = 1;
    }
}

void idx_up(void)
{
    if (idx_req != 1) {
        if (idx_head >= file_menu_get_max() - 5 && idx_column == 4) return;
        idx_req = 1;
        idx_column++;
        if (idx_column >= 5) {
            if (idx_head + 5 >= file_menu_get_max() - 5) {
                idx_column = 4;
                idx_head++;
            } else {
                idx_column = 0;
                idx_head += 5;
            }
        }
    }
}

void idx_down(void)
{
    if (idx_req != 1) {
        if (idx_head == 0 && idx_column == 0) return;
        idx_req = 1;
        idx_column--;
        if (idx_column < 0) {
            if (idx_head - 5 < 0) {
                idx_column = 0;
                idx_head--;
            } else {
                idx_column = 4;
                idx_head -= 5;
            }
        }
    } 
}

void idx_head_fast_up(void)
{
    if (idx_req != 1) {
        if (idx_head >= file_menu_get_max() - 5 && idx_column == 4) return;
        idx_req = 1;
        if (idx_head + 5 >= file_menu_get_max() - 5) {
            idx_head = file_menu_get_max() - 5;
        } else {
            idx_head += 5;
        }
    }
}

void idx_head_fast_down(void)
{
    if (idx_req != 1) {
        if (idx_head == 0 && idx_column == 0) return;
        idx_req = 1;
        if (idx_head - 5 < 0) {
            idx_head = 0;
        } else {
            idx_head -= 5;
        }
    } 
}

void aud_pause(void)
{
    if (aud_req == 0) {
        aud_req = 1;
    }
}

void aud_stop(void)
{
    if (aud_req == 0) {
        aud_req = 2;
    }
}

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
            if (mode == FileView) {
                idx_open();
            } else if (mode == Play) {
                aud_pause();
            }
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_up();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_down();
            } else if (mode == Play) {
                volume_down();
            }
        }
    } else if (button_repeat_count > 20) {
        if (button == HP_BUTTON_CENTER) {
            if (mode == Play) {
                aud_stop();
            }
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_head_fast_up();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_head_fast_down();
            } else if (mode == Play) {
                volume_down();
            }
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

void LCD_Scroll_ShowString(u16 x, u16 y, u8 *p, u16 color, uint16_t *sft_val, int hold_release)
{
    if (*sft_val == 0) {
        if (LCD_ShowStringLnOL(x,  y, (u8 *) p, color) && hold_release) {
            LCD_ShowStringLn(x,  y, (u8 *) p, color);
            (*sft_val)++;
        }
    } else {
        if (LCD_ShowStringLn(x + (8-(*sft_val)%8)%8,  y, (u8 *) &p[((*sft_val)+7)/8], color)) {
            (*sft_val)++;
        } else if (hold_release) {
            LCD_ShowStringLn(x,  y, (u8 *) p, color);
            *sft_val = 0;
        }
    }
}

int main(void)
{
    int count = 0;
    FATFS fs;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    char lcd_str[21];
    stack_data_t item;
    int i;
    const audio_info_type *audio_info;
    int res;
    uint8_t cur_min, cur_sec;
    //uint8_t ttl_min, ttl_sec;
    uint16_t sft_ttl = 0;
    uint16_t sft_art = 0;
    uint16_t sft_alb = 0;

    // LED Pin Setting  LEDR: PC13, LEDG: PA1, LEDB: PA2
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

    // LED All Off
    LEDR(1);
    LEDG(1);
    LEDB(1);

    // Debug Output (PA3 for Proving) by PA_OUT(3, s)
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);

    init_uart0();
    adc_init();
    timer_irq_init();

    // init OLED
    Lcd_Init();
    LCD_Clear(WHITE);
    BACK_COLOR=WHITE;

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
    fr = f_open(&fil, "logo.bin", FA_READ);
    if (fr) printf("open error: %d!\n\r", (int)fr);
    for (i = 0; i < 2; i++) {
        fr = f_read(&fil, image, sizeof(image), &br);
        LCD_ShowPicture(0, 40*i, 159, 40*(i+1)-1);
    }
    delay_1ms(500);
    f_close(&fil);

    // Clear Logo
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    printf("Longan Player ver 1.00\n\r");

    // Search Directories / Files
    stack_t *stack = stack_init();
    file_menu_open_dir("");
    for (;;) {
        if (aud_req == 1) {
            audio_pause();
            aud_req = 0;
        } else if (aud_req == 2) {
            audio_stop();
            mode = FileView;
            LCD_Clear(BLACK);
            BACK_COLOR=BLACK;
            aud_req = 0;
            idx_req = 1;
        } else if (idx_req_open) {
            if (file_menu_is_dir(idx_head+idx_column) > 0) { // Directory
                if (idx_head+idx_column > 0) { // normal directory
                    item.head = idx_head;
                    item.column = idx_column;
                    stack_push(stack, &item);
                }
                file_menu_ch_dir(idx_head+idx_column);
                if (idx_head+idx_column == 0) { // upper ("..") dirctory
                    stack_pop(stack, &item);
                    idx_head = item.head;
                    idx_column = item.column;
                } else { // normal directory
                    idx_head = 0;
                    idx_column = 0;
                }
                idx_req = 1;
            } else { // File
                file_menu_full_sort();
                idx_play = idx_head + idx_column;
                memset(file_str, 0, sizeof(file_str));
                file_menu_get_fname(idx_play, file_str, sizeof(file_str)-1);
                audio_init();
                res = audio_add_playlist_wav(file_str);
                if (res == 1) {
                    mode = Play;
                    { // Load cover art
                        fr = f_open(&fil, "cover.bin", FA_READ);
                        if (fr == FR_OK) {
                            fr = f_read(&fil, image, sizeof(image), &br);
                            f_close(&fil);
                            cover_exists = 1;
                        } else {
                            cover_exists = 0;
                            printf("open error: cover.bin %d!\n\r", (int)fr);
                        }
                    }
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    idx_play++;
                    memset(file_str, 0, sizeof(file_str));
                    file_menu_get_fname(idx_play, file_str, sizeof(file_str)-1);
                    audio_play();
                    idx_play_count = 0;
                } else {
                    audio_stop();
                }
            }
            idx_req_open = 0;
        } else if (idx_req) {
            for (i = 0; i < 5; i++) {
                memset(lcd_str, 0, sizeof(lcd_str));
                strncpy(lcd_str, "                 ", 18);
                LCD_ShowString(8*2, 16*i, (u8 *) lcd_str, BLACK);
                file_menu_get_fname(idx_head+i, lcd_str, 17);
                if (i == idx_column) {
                    LCD_ShowString(8*2, 16*i, (u8 *) lcd_str, GBLUE);
                } else {
                    LCD_ShowString(8*2, 16*i, (u8 *) lcd_str, WHITE);
                }
            }
            idx_req = 0;
            idx_idle_count = 0;
        } else {
            if (mode == Play) {
                if (idx_play < file_menu_get_max()) {
                    res = audio_add_playlist_wav(file_str);
                    if (res == 1 || res == -1) {
                        printf("file: %s\n\r", file_str);
                        idx_play++;
                        memset(file_str, 0, sizeof(file_str));
                        file_menu_get_fname(idx_play, file_str, sizeof(file_str)-1);
                    }
                } else if (!audio_is_playing_or_pausing()) {
                    audio_stop();
                    mode = FileView;
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    idx_req = 1;
                    continue;
                }
                if (!cover_exists || idx_play_count % 128 < 96) {
                    audio_info = audio_get_info();
                    LCD_Scroll_ShowString(8*0, 16*0, (u8 *) audio_info->title, LIGHTBLUE, &sft_ttl, (idx_play_count % 16 == 0));
                    LCD_Scroll_ShowString(8*0, 16*1, (u8 *) audio_info->artist, LIGHTGREEN, &sft_art, (idx_play_count % 16 == 0));
                    LCD_Scroll_ShowString(8*0, 16*2, (u8 *) audio_info->album, GRAYBLUE, &sft_alb, (idx_play_count % 16 == 0));

                    cur_min = (audio_info->offset/44100/4) / 60;
                    cur_sec = (audio_info->offset/44100/4) % 60;
                    /*
                    ttl_min = (audio_info->size/44100/4) / 60;
                    ttl_sec = (audio_info->size/44100/4) % 60;
                    sprintf(lcd_str, "%3d:%02d/%3d:%02d VOL%3d", cur_min, cur_sec, ttl_min, ttl_sec, volume_get());
                    LCD_ShowString(8*0, 16*4, (u8 *) lcd_str, WHITE);
                    */
                    sprintf(lcd_str, "%3d:%02d", cur_min, cur_sec);
                    LCD_ShowString(8*0, 16*4, (u8 *) lcd_str, GRAY);
                    sprintf(lcd_str, "VOL%3d", volume_get());
                    LCD_ShowString(8*14, 16*4, (u8 *) lcd_str, GRAY);
                } else if (cover_exists && idx_play_count % 128 == 96) {
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    LCD_ShowDimPicture(0,0,0+79,79, 32);
                    LCD_ShowDimPicture(80,0,80+79,79, 32);
                    LCD_ShowPicture(40,0,40+79,79);
                } else if (cover_exists && idx_play_count % 128 == 127) {
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    LCD_ShowDimPicture(0,0,0+79,79, 48);
                    LCD_ShowDimPicture(80,0,80+79,79, 48);
                }
                idx_play_count++;
            } else {
                idx_idle_count++;
                if (idx_idle_count > 100) {
                    file_menu_idle();
                }
            }
        }
        delay_1ms(100);
    }
    file_menu_close_dir();
}







