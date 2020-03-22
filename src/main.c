#include <string.h>
#include <time.h>
#include "lcd/my_lcd.h"
#include "fatfs/tf_card.h"
#include "fatfs/ff_util.h"
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"
#include "fifo/stack.h"

unsigned char image[160*80*2/2];
uint32_t count10ms = 0;
uint32_t count_sec = 0;
uint16_t bat_lvl = 99; // 0 ~ 99

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

uint32_t button_prv = HP_BUTTON_OPEN;
uint32_t button_repeat_count = 0;

void idx_open(void)
{
    if (idx_req_open != 1) {
        idx_req_open = 1;
    }
}

void idx_random_open(void)
{
    if (idx_req_open != 2) {
        idx_req_open = 2;
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
        if (idx_column == 0) {
            if (idx_head - 5 < 0) {
                idx_column = 0;
                idx_head--;
            } else {
                idx_column = 4;
                idx_head -= 5;
            }
        } else {
            idx_column--;
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

void power_off(char *msg, int is_error)
{
    int i;
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    if (is_error) {
        LCD_ShowString(24,  0, (u8 *)(msg), BLACK);
        LCD_ShowString(24, 16, (u8 *)(msg), BLUE);
        LCD_ShowString(24, 32, (u8 *)(msg), BRED);
        LCD_ShowString(24, 48, (u8 *)(msg), GBLUE);
        LCD_ShowString(24, 64, (u8 *)(msg), RED);
        for (i = 0; i < 10; i++) {
            LEDR_TOG;
            delay_1ms(200);
            LEDG_TOG;
            delay_1ms(200);
            LEDB_TOG;
            delay_1ms(200);
        }
    }
    PC_OUT(14, 0); // Power Off
    while (1);
    //eclic_system_reset();
}

void tick_100ms(void)
{
    uint32_t button = adc0_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
    //} else if (button != button_prv) {
    } else if (button_prv == HP_BUTTON_OPEN) { // push
        if (button == HP_BUTTON_CENTER) {
            if (mode == FileView) {
                idx_open();
            } else if (mode == Play) {
                aud_pause();
            }
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_dec();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_inc();
            } else if (mode == Play) {
                volume_down();
            }
        }
    } else if (button_repeat_count == 10) { // long push
        if (button == HP_BUTTON_CENTER) {
            if (mode == Play) {
                aud_stop();
            }
            button_repeat_count++; // only once
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_fast_dec();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_fast_inc();
            } else if (mode == Play) {
                volume_down();
            }
        }
    } else if (button_repeat_count == 30) { // long long push
        if (button == HP_BUTTON_CENTER) {
            idx_random_open();
        }
        button_repeat_count++; // only once
    } else if (button_repeat_count == 120) { // long long long push
        if (button == HP_BUTTON_CENTER) {
            power_off("", 0);
        }
        button_repeat_count++; // only once
    } else if (button == button_prv) {
        button_repeat_count++;
    }
    button_prv = button;
}

void tick_1sec(void)
{
    LEDG(0);
    if (count_sec > 5) {
        // Battery value update
        bat_lvl = adc1_get_bat_x100();
    } else {
        adc1_get_bat_x100();
    }
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
        count_sec++;
    }
    timer_interrupt_flag_clear(TIMER0, TIMER_INT_FLAG_UP);
}

void show_battery(uint16_t x, uint16_t y)
{
    uint16_t color;
    if (bat_lvl >= 80) {
        color = 0x0600;
    } else if (bat_lvl < 20) {
        color = 0xc000;
    } else { // if under 80%, gradation from green (0x0600: 80%) to red (0xc000: 0%)
        color = (((0x18 * (60 - (bat_lvl-20)) / 60) & 0x1f) << 11) | (((0x30 * (bat_lvl-20) / 60) & 0x3f) << 5);
    }
    LCD_ShowIcon(x, y, ICON16x16_BATTERY, 1, color);
    if (bat_lvl/10 < 9) {
        LCD_Fill(x+4, y+4, x+11, y+13-bat_lvl/10-1, BLACK);
    }
    LCD_Fill(x+4, y+13-bat_lvl/10, x+11, y+13, color);

    if (bat_lvl < 3) { // Low Battery
        power_off("Low Battery!", 1);
    }
}

void set_backlight_on(void)
{
    // LCD BackLight On (PB7: 1: bright, Hi-Z: dark)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
    PB_OUT(7, 1);
}

void set_backlight_dark(void)
{
    // LCD BackLight dark (PB7: 1: bright, Hi-Z: dark)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
}

int main(void)
{
    int count = 0;
    FATFS fs;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    char lcd_str[8];
    stack_t *stack;
    stack_data_t item;
    int stack_count;
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

    /*
    // Debug Output (PA3 for Proving) by PA_OUT(3, s)
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
    */

    init_uart0();

    // init OLED
    Lcd_Init();
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    set_backlight_on();

    // Progress Bar display before stable power-on for 1 sec
    // to avoid unintended power-on when Headphone plug in
    for (i = 0; i < 40; i++) {
        LCD_Fill(i*4, 4*16+8, i*4+3, 4*16+15, GRAY);
        delay_1ms(25);
    }

    // Power On Pin (PC14)
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_14);
    PC_OUT(14, 1);

    adc0_init();
    adc1_init();
    timer_irq_init();

    // Mount FAT
    fr = f_mount(&fs, "", 1); // 0: mount successful ; 1: mount failed
    while (fr == 1) { 
        delay_1ms(10);
        fr = f_mount(&fs, "", 1);
        if (count++ > 100) break;
    }

    if (fr) { // Mount Fail (Loop)
        power_off("No Card Found!", 1);
    }

    printf("Longan Player ver 1.00\n\r");
    printf("SD Card File System = %d\n\r", fs.fs_type); // FS_EXFAT = 4

    // Opening Logo
    LCD_Clear(WHITE);
    BACK_COLOR=WHITE;
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

    // DAC MCLK out from CK_OUT0 (PA8)
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    rcu_ckout0_config(RCU_CKOUT0SRC_CKPLL2_DIV2);

    // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    PB_OUT(6, 1);

    // Random Seed
    printf("Random seed = %d\n\r", i = adc0_get_raw_data() * adc0_get_raw_data() - adc0_get_raw_data());
    srand(i);

    // Search Directories / Files
    stack = stack_init();
    file_menu_open_dir("/");
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
        } else if (idx_req_open == 1) {
            if (file_menu_is_dir(idx_head+idx_column) > 0) { // Directory
                if (idx_head+idx_column > 0) { // normal directory
                    item.head = idx_head;
                    item.column = idx_column;
                    stack_push(stack, &item);
                }
                if (idx_head+idx_column == 0) { // upper ("..") dirctory
                    if (stack_get_count(stack) > 0) {
                        if (fs.fs_type == FS_EXFAT) { // This is for FatFs known bug for ".." in EXFAT
                            while (stack_get_count(stack) > 1) {
                                stack_pop(stack, &item);
                            }
                            file_menu_close_dir();
                            file_menu_open_dir("/"); // Root directory
                        } else {
                            file_menu_ch_dir(idx_head+idx_column);
                        }
                    } else { // Already in top directory
                        file_menu_close_dir();
                        file_menu_open_dir("/"); // Root directory
                        item.head = 0;
                        item.column = 0;
                        stack_push(stack, &item);
                    }
                    stack_pop(stack, &item);
                    idx_head = item.head;
                    idx_column = item.column;
                } else { // normal directory
                    file_menu_ch_dir(idx_head+idx_column);
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
        } else if (idx_req_open == 2) {
            if (!audio_is_playing_or_pausing()) {
                audio_stop();
            }
            stack_count = stack_get_count(stack);
            if (stack_count > 0) { // Random Play for same level folder
                for (i = 0; i < stack_count; i++) { // chdir to parent directory
                    if (fs.fs_type == FS_EXFAT) { // This is for FatFs known bug for ".." in EXFAT
                        file_menu_close_dir();
                        file_menu_open_dir("/"); // Root directory
                    } else {
                        file_menu_ch_dir(0); // ".."
                    }
                    stack_pop(stack, &item);
                }
                for (i = 0; i < stack_count; i++) { // chdir to child directory at random
                    idx_head = (rand() % (file_menu_get_size() - 1)) + 1;
                    idx_column = 0;
                    file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column);
                    while (file_menu_is_dir(idx_head+idx_column) <= 0) { // not directory
                        idx_head = (idx_head < file_menu_get_size() - 1) ? idx_head + 1 : 1;
                        file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column);
                    }
                    printf("[random_play] dir level: %d, idx: %d, name: %s\n\r", i, idx_head + idx_column, file_menu_get_fname_ptr(idx_head + idx_column));
                    file_menu_ch_dir(idx_head + idx_column);
                    item.head = idx_head;
                    item.column = idx_column;
                    stack_push(stack, &item);
                }
                idx_head = 1;
                idx_column = 0;
                idx_req_open = 1;
            }
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
                    idx_idle_count = 0;
                    continue;
                } else if (audio_is_pausing()) {
                    idx_idle_count++;
                } else {
                    idx_idle_count = 0;
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
                    if (audio_info->filename[0] != '\0') {
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
                        // Level Meter L
                        LCD_Fill(8*0, 16*3+0, (LCD_W-1)*audio_info->lvl_l/100, 16*3+0 + 4, DARKGRAY);
                        LCD_Fill((LCD_W-1)*audio_info->lvl_l/100, 16*3+0, LCD_W-1, 16*3+0 + 4, BLACK);
                        // Level Meter R
                        LCD_Fill(8*0, 16*3+8, (LCD_W-1)*audio_info->lvl_r/100, 16*3+8 + 4, DARKGRAY);
                        LCD_Fill((LCD_W-1)*audio_info->lvl_r/100, 16*3+8, LCD_W-1, 16*3+8 + 4, BLACK);
                        // Progress Bar
                        if (audio_info->data_size != 0) {
                            progress = 159UL * (audio_info->data_offset/1024) / (audio_info->data_size/1024); // for avoid overflow
                            LCD_DrawLine(progress, 16*4-1, 159, 16*4-1, GRAY);
                            LCD_DrawLine(0, 16*4-1, progress, 16*4-1, BLUE);
                        }
                        // Track Number
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
                        if (!audio_is_pausing() || idx_play_count % 8 < 6) { // Elapsed Time blinks when pausing
                            sprintf(lcd_str, "%3d:%02d", cur_min, cur_sec);
                            LCD_ShowString(8*5, 16*4, (u8 *) lcd_str, GRAY);
                        } else {
                            LCD_ShowString(8*5, 16*4, (u8 *) "      ", GRAY);
                        }
                    }
                    // Volume
                    LCD_ShowIcon(8*12, 16*4, ICON16x16_VOLUME, 1, GRAY);
                    sprintf(lcd_str, "%3d", volume_get());
                    LCD_ShowString(8*14, 16*4, (u8 *) lcd_str, GRAY);
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
            }
            if (idx_idle_count < 10 * 60 * 1) {
                set_backlight_on();
            } else if (idx_idle_count < 10 * 60 * 3) {
                set_backlight_dark();
            } else { // Auto Power-off in 3 min
                power_off("", 0);
            }
        }
        // Battery
        show_battery(8*18, 16*4);
        delay_1ms(100);
    }
    file_menu_close_dir();
}







