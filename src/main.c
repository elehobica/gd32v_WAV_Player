#include "lcd/my_lcd.h"
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

uint16_t idx_head = 0;
uint16_t idx_column = 0;
uint32_t idx_idle_count = 0;
uint32_t idx_play_count = 0;
int cover_exists = 0;
uint32_t data_offset_prv = 0;

void idx_open(void)
{
    if (idx_req_open != 1) {
        idx_req_open = 1;
    }
}

void idx_inc(void)
{
    if (idx_req != 1) {
        if (idx_head >= file_menu_get_size() - 5 && idx_column == 4) return;
        if (idx_head + idx_column + 1 >= file_menu_get_size()) return;
        idx_req = 1;
        idx_column++;
        if (idx_column >= 5) {
            if (idx_head + 5 >= file_menu_get_size() - 5) {
                idx_column = 4;
                idx_head++;
            } else {
                idx_column = 0;
                idx_head += 5;
            }
        }
    }
}

void idx_dec(void)
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

void idx_fast_inc(void)
{
    if (idx_req != 1) {
        if (idx_head >= file_menu_get_size() - 5 && idx_column == 4) return;
        if (idx_head + idx_column + 1 >= file_menu_get_size()) return;
        if (idx_head + 5 >= file_menu_get_size() - 5) {
            idx_head = file_menu_get_size() - 5;
            idx_inc();
        } else {
            idx_head += 5;
        }
        idx_req = 1;
    }
}

void idx_fast_dec(void)
{
    if (idx_req != 1) {
        if (idx_head == 0 && idx_column == 0) return;
        if (idx_head - 5 < 0) {
            idx_head = 0;
            idx_dec();
        } else {
            idx_head -= 5;
        }
        idx_req = 1;
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
                idx_inc();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_dec();
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
                idx_fast_inc();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_fast_dec();
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

int main(void)
{
    int count = 0;
    FATFS fs;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    char lcd_str[8];
    stack_data_t item;
    int i;
    uint16_t progress;
    const audio_info_type *audio_info;
    uint8_t cur_min, cur_sec;
    //uint8_t ttl_min, ttl_sec;
    uint16_t sft_ttl = 0;
    uint16_t sft_art = 0;
    uint16_t sft_alb = 0;
    uint16_t sft_num = 0;

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
                // After audio_init(), Never call file_menu_xxx() functions!
                // Otherwise, it causes conflict between main and int routine
                audio_init();
                mode = Play;
                // Load cover art
                fr = f_open(&fil, "cover.bin", FA_READ);
                if (fr == FR_OK) {
                    fr = f_read(&fil, image, sizeof(image), &br);
                    f_close(&fil);
                    cover_exists = 1;
                } else {
                    cover_exists = 0;
                    printf("open error: cover.bin %d!\n\r", (int)fr);
                }
                LCD_Clear(BLACK);
                BACK_COLOR=BLACK;
                audio_play(idx_head + idx_column);
                idx_play_count = 0;
                idx_idle_count = 0;
            }
            idx_req_open = 0;
        } else if (idx_req) {
            for (i = 0; i < 5; i++) {
                if (idx_head+i >= file_menu_get_size()) {
                    LCD_ShowString(8*0, 16*i, (u8 *) "                    ", BLACK);
                    continue;
                }
                if (file_menu_is_dir(idx_head+i)) {
                    LCD_ShowIcon(8*0, 16*i, ICON16x16_FOLDER, 0, GRAY);
                } else {
                    LCD_ShowIcon(8*0, 16*i, ICON16x16_FILE, 0, GRAY);
                }
                LCD_ShowString(8*2, 16*i, (u8 *) "                  ", BLACK);
                if (i == idx_column) {
                    LCD_ShowStringLn(8*2, 16*i, 8*2, LCD_W-1, (u8 *) file_menu_get_fname_ptr(idx_head+i), GBLUE);
                } else {
                    LCD_ShowStringLn(8*2, 16*i, 8*2, LCD_W-1, (u8 *) file_menu_get_fname_ptr(idx_head+i), WHITE);
                }
            }
            idx_req = 0;
            idx_idle_count = 0;
        } else {
            if (mode == Play) {
                if (!audio_is_playing_or_pausing()) {
                    audio_stop();
                    mode = FileView;
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    idx_req = 1;
                    continue;
                }
                if (!cover_exists || idx_play_count % 128 < 96) {
                    audio_info = audio_get_info();
                    if (data_offset_prv == 0 || data_offset_prv > audio_info->data_offset) { // when changing to next title
                        //LCD_Clear(BLACK);
                        //BACK_COLOR=BLACK;
                        if (cover_exists) {
                            LCD_ShowDimPicture(0,0,0+79,79, 48);
                            LCD_ShowDimPicture(80,0,80+79,79, 48);
                        }
                        sft_ttl = 0;
                        sft_art = 0;
                        sft_alb = 0;
                        sft_num = 0;
                    }
                    if (audio_info->title[0] != '\0') {
                        LCD_ShowIcon(8*0, 16*0, ICON16x16_TITLE, 1, GRAY);
                        LCD_Scroll_ShowString(8*2, 16*0, 8*2, LCD_W-1, (u8 *) audio_info->title, LIGHTBLUE, &sft_ttl, idx_play_count);
                    } else if (audio_info->filename[0] != '\0') {
                        LCD_ShowIcon(8*0, 16*0, ICON16x16_FILE, 1, GRAY);
                        LCD_Scroll_ShowString(8*2, 16*0, 8*2, LCD_W-1, (u8 *) audio_info->filename, LIGHTBLUE, &sft_ttl, idx_play_count);
                    }
                    if (audio_info->artist[0] != '\0') {
                        LCD_ShowIcon(8*0, 16*1, ICON16x16_ARTIST, 1, GRAY);
                        LCD_Scroll_ShowString(8*2, 16*1, 8*2, LCD_W-1, (u8 *) audio_info->artist, LIGHTGREEN, &sft_art, idx_play_count);
                    }
                    if (audio_info->album[0] != '\0') {
                        LCD_ShowIcon(8*0, 16*2, ICON16x16_ALBUM, 1, GRAY);
                        LCD_Scroll_ShowString(8*2, 16*2, 8*2, LCD_W-1, (u8 *) audio_info->album, GRAYBLUE, &sft_alb, idx_play_count);
                    }
                    if (audio_info->data_size != 0) { // Progress Bar
                        progress = 159UL * (audio_info->data_offset/1024) / (audio_info->data_size/1024); // for avoid overflow
                        LCD_DrawLine(progress, 16*4-1, 159, 16*4-1, GRAY);
                        LCD_DrawLine(0, 16*4-1, progress, 16*4-1, BLUE);
                    }
                    if (audio_info->number[0] != '\0') {
                        LCD_Scroll_ShowString(8*0, 16*4, 8*0, 8*5-1, (u8 *) audio_info->number, GRAY, &sft_num, idx_play_count);
                    }
                    // Elapsed Time
                    cur_min = (audio_info->data_offset/44100/4) / 60;
                    cur_sec = (audio_info->data_offset/44100/4) % 60;
                    /*
                    ttl_min = (audio_info->data_size/44100/4) / 60;
                    ttl_sec = (audio_info->data_size/44100/4) % 60;
                    sprintf(lcd_str, "%3d:%02d/%3d:%02d VOL%3d", cur_min, cur_sec, ttl_min, ttl_sec, volume_get());
                    LCD_ShowString(8*0, 16*4, (u8 *) lcd_str, WHITE);
                    */
                    sprintf(lcd_str, "%3d:%02d", cur_min, cur_sec);
                    LCD_ShowString(8*5, 16*4, (u8 *) lcd_str, GRAY);
                    // Volume
                    LCD_ShowIcon(8*12, 16*4, ICON16x16_VOLUME, 1, GRAY);
                    sprintf(lcd_str, "%3d", volume_get());
                    LCD_ShowString(8*14, 16*4, (u8 *) lcd_str, GRAY);
                    // Battery
                    LCD_ShowIcon(8*18, 16*4, ICON16x16_BATTERY, 1, DARKGREEN);
                    data_offset_prv = audio_info->data_offset;
                } else if (cover_exists && idx_play_count % 128 == 96) {
                    LCD_Clear(BLACK);
                    BACK_COLOR=BLACK;
                    LCD_ShowDimPicture(0,0,0+79,79, 32);
                    LCD_ShowDimPicture(80,0,80+79,79, 32);
                    LCD_ShowPicture(40,0,40+79,79);
                /*
                } else if (cover_exists && idx_play_count % 128 >= 113 && idx_play_count % 128 < 127) {
                    // Cover Art Fade out
                    LCD_ShowDimPicture(40,0,40+79,79, (128 - (idx_play_count % 128))*16);
                */
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
                if (idx_idle_count > 10 * 60 * 10) { // auto repeat in 10 min
                    idx_req_open = 1;
                }
            }
        }
        delay_1ms(100);
    }
    file_menu_close_dir();
}







