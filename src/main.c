#include <string.h>
#include <time.h>
#include "lcd/my_lcd.h"
#include "fatfs/tf_card.h"
#include "fatfs/ff_util.h"
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"
#include "timer_pwm.h"
#include "fifo/stack.h"
#include "board_conf.h"

#if defined(BOARD_SIPEED_LONGAN_NANO)
#define BLANK_LINE ((uint8_t *) "                    ")
#define NUM_IDX_ITEMS 5
// Cover Art
const uint16_t ca_x = 8*0;
const uint16_t ca_y = 16*0;
// Title
const uint16_t ti_x = 8*0;
const uint16_t ti_y = 16*0;
// Artist
const uint16_t ar_x = 8*0;
const uint16_t ar_y = 16*1;
// Album
const uint16_t al_x = 8*0;
const uint16_t al_y = 16*2;
// Level Meter
const uint16_t lm_x = 8*0;
const uint16_t lm_y = 16*3;
// Progress Bar
const uint16_t pb_x = 8*0;
const uint16_t pb_y = 16*4-1;
const uint16_t pb_h = 1;
// Track Number
const uint16_t tn_x = 8*0;
const uint16_t tn_y = 16*4;
// Elapsed Time
const uint16_t et_x = 8*5;
const uint16_t et_y = 16*4;
// Volume
const uint16_t vl_x = 8*12;
const uint16_t vl_y = 16*4;
#elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
#define BLANK_LINE ((uint8_t *) "                              ")
#define NUM_IDX_ITEMS 8
// Cover Art
const uint16_t ca_x = 8*0;
const uint16_t ca_y = 16*0;
// Title
const uint16_t ti_x = 8*10;
const uint16_t ti_y = 16*1;
// Artist
const uint16_t ar_x = 8*10;
const uint16_t ar_y = 16*2;
// Album
const uint16_t al_x = 8*10;
const uint16_t al_y = 16*3;
// Level Meter
const uint16_t lm_x = 8*0;
const uint16_t lm_y = 16*5+8+2;
// Progress Bar
const uint16_t pb_x = 8*0;
const uint16_t pb_y = 16*7+2;
const uint16_t pb_h = 2;
// Track Number
const uint16_t tn_x = 8*0;
const uint16_t tn_y = LCD_H-16;
// Elapsed Time
const uint16_t et_x = 8*5;
const uint16_t et_y = LCD_H-16;
// Volume
const uint16_t vl_x = 8*22;
const uint16_t vl_y = LCD_H-16;
#endif

#define NUM_BTN_HISTORY     30
#define FLASH_PAGE_SIZE     0x400
#define FLASH_PAGE127       (FLASH_BASE + FLASH_PAGE_SIZE*127)
#define CFG_SIZE            0x80
#define CFG_VERSION         (FLASH_PAGE127 + 0x380)
#define CFG_VOLUME          (FLASH_PAGE127 + 0x384)
#define CFG_STACK_COUNT     (FLASH_PAGE127 + 0x388)
#define CFG_STACK_DATA0     (FLASH_PAGE127 + 0x38c)
#define CFG_STACK_DATA1     (FLASH_PAGE127 + 0x390)
#define CFG_STACK_DATA2     (FLASH_PAGE127 + 0x394)
#define CFG_STACK_DATA3     (FLASH_PAGE127 + 0x398)
#define CFG_STACK_DATA4     (FLASH_PAGE127 + 0x39c)
#define CFG_IDX             (FLASH_PAGE127 + 0x3a0)
#define CFG_MODE            (FLASH_PAGE127 + 0x3a4)
#define CFG_IDX_PLAY        (FLASH_PAGE127 + 0x3a8)
#define CFG_DATA_OFFSET     (FLASH_PAGE127 + 0x3ac)
#define CFG_SEED            (FLASH_PAGE127 + 0x3b0)
#define CFG32(x)            REG32(x)

int version;
//unsigned char image[160*80*2/2];
unsigned char *image[8] = {}; // 80*10*2
uint32_t count10ms = 0;
uint32_t count1sec = 0;
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

uint8_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN
uint32_t button_repeat_count = 0;
stack_t *stack; // for change directory history

void idx_open(void)
{
    if (idx_req_open != 1) {
        idx_req_open = 1;
    }
}

void idx_close(void)
{
    if (idx_req_open != 1) {
        idx_column = 0;
        idx_head = 0;
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
        if (idx_head >= file_menu_get_size() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) return;
        if (idx_head + idx_column + 1 >= file_menu_get_size()) return;
        idx_req = 1;
        idx_column++;
        if (idx_column >= NUM_IDX_ITEMS) {
            if (idx_head + NUM_IDX_ITEMS >= file_menu_get_size() - NUM_IDX_ITEMS) {
                idx_column = NUM_IDX_ITEMS-1;
                idx_head++;
            } else {
                idx_column = 0;
                idx_head += NUM_IDX_ITEMS;
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
            if (idx_head - NUM_IDX_ITEMS < 0) {
                idx_column = 0;
                idx_head--;
            } else {
                idx_column = NUM_IDX_ITEMS-1;
                idx_head -= NUM_IDX_ITEMS;
            }
        } else {
            idx_column--;
        }
    } 
}

void idx_fast_inc(void)
{
    if (idx_req != 1) {
        if (idx_head >= file_menu_get_size() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) return;
        if (idx_head + idx_column + 1 >= file_menu_get_size()) return;
        if (file_menu_get_size() < NUM_IDX_ITEMS) {
            idx_inc();
        } else if (idx_head + NUM_IDX_ITEMS >= file_menu_get_size() - NUM_IDX_ITEMS) {
            idx_head = file_menu_get_size() - NUM_IDX_ITEMS;
            idx_inc();
        } else {
            idx_head += NUM_IDX_ITEMS;
        }
        idx_req = 1;
    }
}

void idx_fast_dec(void)
{
    if (idx_req != 1) {
        if (idx_head == 0 && idx_column == 0) return;
        if (idx_head - NUM_IDX_ITEMS < 0) {
            idx_head = 0;
            idx_dec();
        } else {
            idx_head -= NUM_IDX_ITEMS;
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

void tick_10ms(void)
{
    LEDG(1);
}

void power_off(char *msg, int is_error)
{
    int i;
    stack_data_t item;

    // timer tick stop
    timer0_irq_stop();
    // save flash config
    if (!is_error) {
        unsigned int *data = (unsigned int *) malloc(FLASH_PAGE_SIZE - CFG_SIZE);
        for (i = 0; i < FLASH_PAGE_SIZE - CFG_SIZE; i += 4) {
            data[i/4] = CFG32(FLASH_PAGE127 + i); // this is not configuration values but flash values
        }
        fmc_unlock();
        fmc_page_erase(FLASH_PAGE127);
        for (i = 0; i < FLASH_PAGE_SIZE - CFG_SIZE; i += 4) { // write back 1024-128 Byte
            fmc_word_program(FLASH_PAGE127 + i, data[i/4]);
        }
        free(data);
        // flash config
        fmc_word_program(CFG_VERSION, version);
        fmc_word_program(CFG_VOLUME, volume_get());
        fmc_word_program(CFG_STACK_COUNT, stack_get_count(stack));
        for (i = 0; i < CFG32(CFG_STACK_COUNT); i++) {
            stack_pop(stack, &item);
            fmc_word_program(CFG_STACK_DATA0 + i*4, ((uint32_t) item.column << 16) | (uint32_t) item.head);
        }
        fmc_word_program(CFG_IDX, ((uint32_t) idx_column << 16) | (uint32_t) idx_head);
        fmc_word_program(CFG_MODE, mode);
        fmc_word_program(CFG_IDX_PLAY, audio_get_idx_play());
        fmc_word_program(CFG_DATA_OFFSET, audio_get_info()->data_offset);
        fmc_word_program(CFG_SEED, rtc_counter_get());
        fmc_lock();
        printf("Saved flash config\n\r");
    }

    // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    PB_OUT(6, 0);

    // LED signal
    if (is_error) {
        LEDR(0); // LED Red Light
    } else {
        LEDG(0); // LED Green Light
    }
    LCD_Clear(BLACK);
    if (strlen(msg) > 0) {
        LCD_ShowString(24,  0, (u8 *) msg, BLACK);
        LCD_ShowString(24, 16, (u8 *) msg, BLUE);
        LCD_ShowString(24, 32, (u8 *) msg, BRED);
        LCD_ShowString(24, 48, (u8 *) msg, GBLUE);
        LCD_ShowString(24, 64, (u8 *) msg, RED);
        delay_1ms(1000);
    }
    PC_OUT(14, 0); // Power Off
    while (1);
    //eclic_system_reset();
}

int count_center_clicks(void)
{
    int i;
    int detected_fall = 0;
    int count = 0;
    for (i = 0; i < 4; i++) {
        if (button_prv[i] != HP_BUTTON_OPEN) {
            return 0;
        }
    }
    for (i = 4; i < NUM_BTN_HISTORY; i++) {
        if (detected_fall == 0 && button_prv[i-1] == HP_BUTTON_OPEN && button_prv[i] == HP_BUTTON_CENTER) {
            detected_fall = 1;
        } else if (detected_fall == 1 && button_prv[i-1] == HP_BUTTON_CENTER && button_prv[i] == HP_BUTTON_OPEN) {
            count++;
            detected_fall = 0;
        }
    }
    if (count > 0) {
        for (i = 0; i < NUM_BTN_HISTORY; i++) button_prv[i] = HP_BUTTON_OPEN;
    }
    return count;
}

void tick_100ms(void)
{
    int i;
    int center_clicks;
    // Center Button: event timing is at button release
    // Other Buttons: event timing is at button push
    uint8_t button = adc0_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
        center_clicks = count_center_clicks(); // must be called once per tick because button_prv[] status has changed
        if (center_clicks > 0) {
            printf("clicks =  %d\n\r", center_clicks);
            if (mode == FileView) {
                if (center_clicks == 1) {
                    idx_open();
                } else if (center_clicks == 2) {
                    idx_close();
                } else if (center_clicks == 3) {
                    idx_random_open();
                }
            } else if (mode == Play) {
                if (center_clicks == 1) {
                    aud_pause();
                } else if (center_clicks == 2) {
                    aud_stop();
                }
            }
        }
    } else if (button_prv[0] == HP_BUTTON_OPEN) { // push
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
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
            button_repeat_count++; // only once and step to longer push event
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
            power_off("", 0);
        }
        button_repeat_count++; // only once and step to longer push event
    } else if (button == button_prv[0]) {
        button_repeat_count++;
    }
    // Button status shift
    for (i = NUM_BTN_HISTORY-2; i >= 0; i--) {
        button_prv[i+1] = button_prv[i];
    }
    button_prv[0] = button;
}

void tick_1sec(void)
{
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
        count1sec++;
    }
    timer_interrupt_flag_clear(TIMER0, TIMER_INT_FLAG_UP);
}

void show_battery(uint16_t x, uint16_t y)
{
    static int bat_chk_count = 0;
    uint16_t color;

    if (bat_chk_count++ % 100 == 39) {
        PB_OUT(8, 1); // BATTERY CHECK ON
        delay_1ms(1);
        bat_lvl = adc1_get_bat_x100();
        PB_OUT(8, 0); // BATTERY CHECK OFF
    }
    if (bat_lvl >= 40) {
        color = 0x0600;
    } else if (bat_lvl >= 10) {
        color = 0xc600;
    } else {
        color = 0xc000;
    }
    /*
    // gradation from 20% ~ 80%
    {
        // if under 80%, gradation from green (0x0600: 80%) to red (0xc000: 20%)
        color = (((0x18 * (60 - (bat_lvl-20)) / 60) & 0x1f) << 11) | (((0x30 * (bat_lvl-20) / 60) & 0x3f) << 5);
    }
    */
    LCD_ShowIcon(x, y, ICON16x16_BATTERY, 1, color);
    if (bat_lvl/10 < 9) {
        LCD_Fill(x+4, y+4, x+11, y+13-bat_lvl/10-1, BLACK);
    }
    LCD_Fill(x+4, y+13-bat_lvl/10, x+11, y+13, color);

    if (bat_lvl < 3) { // Low Battery
        power_off("Low Battery!", 0);
    }
}

// Optional: Backlight control for PB7 by PWM
void set_backlight_full(void)
{
    #if defined(BOARD_SIPEED_LONGAN_NANO)
    timer3_pwm_set_ratio(80);
    #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
    timer1_pwm_set_ratio(50);
    #endif
}

void set_backlight_dark(void)
{
    #if defined(BOARD_SIPEED_LONGAN_NANO)
    timer3_pwm_set_ratio(8);
    #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
    timer1_pwm_set_ratio(5);
    #endif
}

int main(void)
{
    int count = 0;
    FATFS fs;
    FIL fil;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    char lcd_str[16];
    char *lcd_ptr;
    stack_data_t item;
    int stack_count;
    int i;
    int j;
    uint16_t progress;
    uint16_t idx_play = 0;
    const audio_info_type *audio_info;
    uint8_t cur_min, cur_sec;
    uint8_t ttl_min, ttl_sec;
    // sft_xxx variables are keeping shifting position for string scroll
    uint16_t sft_ttl = 0;
    uint16_t sft_art = 0;
    uint16_t sft_alb = 0;
    uint16_t sft_num = 0;
    uint16_t lvl_l, lvl_r;

    // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    PB_OUT(6, 1);

    // LED Pin Setting  LEDR: PC13, LEDG: PA1, LEDB: PA2
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1|GPIO_PIN_2);

    // LED All Off
    LEDR(1);
    LEDG(1);
    LEDB(1);

    // BATTERY CHECK Pin (0: Not check, 1: Check) (PB8)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    PB_OUT(8, 1);

    /*
    // Debug Output (PA3 for Proving) by PA_OUT(3, s)
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_3);
    */

    init_uart0();
    printf("\n\r");

    // for backlight control
    #if defined(BOARD_SIPEED_LONGAN_NANO)
    timer3_pwm_init();
    #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
    timer1_pwm_init();
    #endif

    // init OLED
    Lcd_Init();
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    set_backlight_full();

    // Progress Bar display before stable power-on for 1 sec
    // to avoid unintended power-on when Headphone plug in
    for (i = 0; i < 40; i++) {
        LCD_Fill(i*LCD_W/40, LCD_H-8, (i+1)*LCD_W/40-1, LCD_H-1, GRAY);
        delay_1ms(25);
    }

    // Power-Keep Pin (PC14)
    rcu_periph_clock_enable(RCU_GPIOC);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_14);
    PC_OUT(14, 1);
    button_repeat_count = 100; // longer than longest push event to avoid more push event

    adc0_init();
    adc1_init();
    adc1_get_bat_x100(); // Battery check idle run

    // Flash config load
    version = CFG32(CFG_VERSION);
    if (version == 0xffffffff) { // if no history written
        printf("Initialize flash config\n\r");
        unsigned int *data = (unsigned int *) malloc(FLASH_PAGE_SIZE - CFG_SIZE);
        for (i = 0; i < FLASH_PAGE_SIZE - CFG_SIZE; i += 4) {
            data[i/4] = CFG32(FLASH_PAGE127 + i);  // this is not configuration values but flash values
        }
        fmc_unlock();
        fmc_page_erase(FLASH_PAGE127);
        for (i = 0; i < FLASH_PAGE_SIZE - CFG_SIZE; i += 4) { // write back 1024-128 Byte
            fmc_word_program(FLASH_PAGE127 + i, data[i/4]);
        }
        free(data);
        // flash config initial values
        fmc_word_program(CFG_VERSION, 100);
        fmc_word_program(CFG_VOLUME, 65);
        fmc_word_program(CFG_STACK_COUNT, 0);
        fmc_word_program(CFG_STACK_DATA0, 0);
        fmc_word_program(CFG_STACK_DATA1, 0);
        fmc_word_program(CFG_STACK_DATA2, 0);
        fmc_word_program(CFG_STACK_DATA3, 0);
        fmc_word_program(CFG_STACK_DATA4, 0);
        fmc_word_program(CFG_IDX, 0);
        fmc_word_program(CFG_MODE, FileView);
        fmc_word_program(CFG_IDX_PLAY, 0);
        fmc_word_program(CFG_DATA_OFFSET, 0);
        fmc_word_program(CFG_SEED, 0);
        fmc_lock();
    } else {
        for (i = 0; i < 128; i += 4) {
            printf("CFG[%d] = 0x%08x\n\r", i/4, (int) CFG32(CFG_VERSION + i));
        }
    }
    volume_set(CFG32(CFG_VOLUME));

    // RTC is used for random seed
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_BKPI);
    rcu_periph_clock_enable(RCU_RTC);
    //rcu_bkp_reset_enable();
    pmu_backup_write_enable();
    RCU_BDCTL |= RCU_BDCTL_RTCEN;
    rcu_rtc_clock_config(RCU_RTCSRC_HXTAL_DIV_128);
    rtc_prescaler_set(0);
    rtc_counter_set(CFG32(CFG_SEED));

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

    printf("Longan Player ver %d.%02d\n\r", (int) CFG32(CFG_VERSION)/100, (int) CFG32(CFG_VERSION)%100);
    printf("SD Card File System = %d\n\r", fs.fs_type); // FS_EXFAT = 4

    // Opening Logo
    LCD_Clear(WHITE);
    for (i = 0; i < 8; i++) {
        image[i] = (unsigned char *) malloc(160*5*2);
    }
    #if defined(BOARD_SIPEED_LONGAN_NANO)
    fr = f_open(&fil, "logo.bin", FA_READ);
    if (fr) {
        printf("open error: logo.bin %d!\n\r", (int)fr);
    } else {
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 8; j++) {
                fr = f_read(&fil, image[j], 160*5*2, &br);
            }
            LCD_ShowPicture((LCD_W-160)/2+0, (LCD_H-80)/2+40*i, (LCD_W-160)/2+159, (LCD_H-80)/2+40*(i+1)-1);
        }
    }
    #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
    fr = f_open(&fil, "lilygo_logo.bin", FA_READ);
    if (fr) {
        printf("open error: lilygo_logo.bin %d!\n\r", (int)fr);
    } else {
        LCD_Address_Set(0, 0, 239, 134);
        j = 0;
        while (j < 240*135*2*10) { // 10 Frames
            fr = f_read(&fil, image[0], 160*5*2, &br);
            for (i = 0; i < br; i++) {
                LCD_WR_DATA8(image[0][i]);
                j++;
            }
        }
        f_close(&fil);
    }
    #endif
    delay_1ms(500);
    for (i = 0; i < 8; i++) {
        free(image[i]);
        image[i] = NULL;
    }
    f_close(&fil);

    // Clear Logo
    LCD_Clear(BLACK);

    timer0_irq_init(); // for TIMER0_UP_IRQHandler

    // DAC MCLK out from CK_OUT0 (PA8)
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8);
    rcu_ckout0_config(RCU_CKOUT0SRC_CKPLL2_DIV2);

    /*
    // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    PB_OUT(6, 1);
    */

    // Random Seed
    printf("Random seed = %lu\n\r", (uint32_t) CFG32(CFG_SEED));
    srand((uint32_t) CFG32(CFG_SEED));

    // Search Directories / Files
    stack = stack_init();
    file_menu_open_dir("/");
    if (file_menu_get_size() <= 1) {
        power_off("Card Read Error!", 1);
    }
    // Restore power off situation (directory, mode, data_offset)
    if (CFG32(CFG_STACK_COUNT) > 0) {
        for (i = CFG32(CFG_STACK_COUNT) - 1; i >= 0; i--) {
            item.head = CFG32(CFG_STACK_DATA0 + i*4) & 0xffff;
            item.column = (CFG32(CFG_STACK_DATA0 + i*4) >> 16) & 0xffff;
            file_menu_sort_entry(item.head+item.column, item.head+item.column + 1);
            if (file_menu_is_dir(item.head+item.column) <= 0 || item.head+item.column == 0) { // Not Directory or Parent Directory
                break;
            }
            stack_push(stack, &item);
            file_menu_ch_dir(item.head+item.column);
        }
    }
    audio_init();
    idx_head = CFG32(CFG_IDX) & 0xffff;
    idx_column = (CFG32(CFG_IDX) >> 16) & 0xffff;
    mode = CFG32(CFG_MODE);
    if (mode == Play) {
        idx_req_open = 1;
        idx_req = 0;
        idx_play = CFG32(CFG_IDX_PLAY);
        audio_set_data_offset(CFG32(CFG_DATA_OFFSET));
    }

    // main loop
    for (;;) {
        if (aud_req == 1) {
            audio_pause();
            aud_req = 0;
        } else if (aud_req == 2) {
            audio_stop();
            for (i = 0; i < 8; i++) {
                free(image[i]);
                image[i] = NULL;
            }
            mode = FileView;
            LCD_Clear(BLACK);
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

                /*
                // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
                rcu_periph_clock_enable(RCU_GPIOB);
                gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
                PB_OUT(6, 1);
                */

                mode = Play;
                // Load cover art
                fr = f_open(&fil, "cover.bin", FA_READ);
                if (fr == FR_OK) {
                    cover_exists = 1;
                    for (i = 0; i < 8; i++) {
                        image[i] = (unsigned char *) malloc(80*10*2);
                        if (image[i] != NULL) {
                            fr = f_read(&fil, image[i], 80*10*2, &br);
                        } else {
                            LEDR(0);
                            cover_exists = 0;
                            break;
                        }
                    }
                    f_close(&fil);
                } else {
                    for (i = 0; i < 8; i++) {
                        image[i] = NULL;
                    }
                    cover_exists = 0;
                    printf("open error: cover.bin %d!\n\r", (int)fr);
                }
                // After audio_play(), Never call file_menu_xxx() functions until it stops.
                // Otherwise, it causes conflict between main and int routine
                if (idx_play > 1) {
                    audio_play(idx_play - 1);
                } else {
                    audio_play(idx_head + idx_column);
                }
                idx_play_count = 0;
                idx_idle_count = 0;
                LCD_Clear(BLACK);
                if (cover_exists) {
                    #if defined(BOARD_SIPEED_LONGAN_NANO)
                    LCD_ShowDimPicture(ca_x, ca_y, ca_x+79, ca_y+79, 48);
                    LCD_ShowDimPicture(ca_x+80, ca_y, ca_x+80+79, ca_y+79, 48);
                    #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                    LCD_ShowPicture(ca_x, ca_y, ca_x+79, ca_y+79);
                    #endif
                }
            }
            idx_req_open = 0;
        } else if (idx_req_open == 2) { // Random Play
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
                    file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column + 1);
                    while (file_menu_is_dir(idx_head+idx_column) <= 0) { // not directory
                        idx_head = (idx_head < file_menu_get_size() - 1) ? idx_head + 1 : 1;
                        file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column + 1);
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
            for (i = 0; i < NUM_IDX_ITEMS; i++) {
                if (idx_head+i >= file_menu_get_size()) {
                    LCD_ShowString(8*0, 16*i, BLANK_LINE, BLACK);
                    continue;
                }
                if (file_menu_is_dir(idx_head+i)) {
                    LCD_ShowIcon(8*0, 16*i, ICON16x16_FOLDER, 0, GRAY);
                } else {
                    LCD_ShowIcon(8*0, 16*i, ICON16x16_FILE, 0, GRAY);
                }
                lcd_ptr = file_menu_get_fname_ptr(idx_head+i);
                if (i == idx_column) {
                    LCD_ShowStringLn(8*2, 16*i, 8*2, LCD_W-1, (u8 *) lcd_ptr, GBLUE);
                } else {
                    LCD_ShowStringLn(8*2, 16*i, 8*2, LCD_W-1, (u8 *) lcd_ptr, WHITE);
                }
                j = strlen(lcd_ptr);
                if (j < (LCD_W)/8-2) {
                    LCD_ShowStringLn(8*(2+j), 16*i, 8*(2+j), LCD_W-1, BLANK_LINE, BLACK);
                }
            }
            idx_req = 0;
            idx_idle_count = 0;
        } else {
            if (mode == Play) {
                if (!audio_is_playing_or_pausing()) {
                    if (audio_play(0)) { // play next file -> prepare to change to next title info
                        LCD_Clear(BLACK);
                        if (cover_exists) {
                            #if defined(BOARD_SIPEED_LONGAN_NANO)
                            LCD_ShowDimPicture(ca_x, ca_y, ca_x+79, ca_y+79, 48);
                            LCD_ShowDimPicture(ca_x+80, ca_y, ca_x+80+79, ca_y+79, 48);
                            #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                            LCD_ShowPicture(ca_x, ca_y, ca_x+79, ca_y+79);
                            #endif
                        }
                        sft_ttl = 0;
                        sft_art = 0;
                        sft_alb = 0;
                        sft_num = 0;
                    } else { // no next file -> play finish
                        for (i = 0; i < 8; i++) {
                            free(image[i]);
                            image[i] = NULL;
                        }
                        mode = FileView;
                        idx_req = 1;
                        idx_idle_count = 0;
                        continue;
                    }
                } else if (audio_is_pausing()) {
                    idx_idle_count++;
                } else {
                    idx_idle_count = 0;
                }
                #if defined(BOARD_SIPEED_LONGAN_NANO)
                if (!cover_exists || idx_play_count % 128 < 96) {
                #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                if (1) {
                #endif
                    audio_info = audio_get_info();
                    if (audio_info->filename[0] != '\0') {
                        if (audio_info->title[0] != '\0') {
                            LCD_ShowIcon(ti_x, ti_y, ICON16x16_TITLE, 1, GRAY);
                            LCD_Scroll_ShowString(ti_x+16, ti_y, ti_x+16, LCD_W-1, (u8 *) audio_info->title, LIGHTBLUE, &sft_ttl, idx_play_count);
                        } else if (audio_info->filename[0] != '\0') {
                            LCD_ShowIcon(ti_x, ti_y, ICON16x16_FILE, 1, GRAY);
                            LCD_Scroll_ShowString(ti_x+16, ti_y, ti_x+16, LCD_W-1, (u8 *) audio_info->filename, LIGHTBLUE, &sft_ttl, idx_play_count);
                        }
                        if (audio_info->artist[0] != '\0') {
                            LCD_ShowIcon(ar_x, ar_y, ICON16x16_ARTIST, 1, GRAY);
                            LCD_Scroll_ShowString(ar_x+16, ar_y, ar_x+16, LCD_W-1, (u8 *) audio_info->artist, LIGHTGREEN, &sft_art, idx_play_count);
                        }
                        if (audio_info->album[0] != '\0') {
                            LCD_ShowIcon(al_x, al_y, ICON16x16_ALBUM, 1, GRAY);
                            LCD_Scroll_ShowString(al_x+16, al_y, al_x+16, LCD_W-1, (u8 *) audio_info->album, GRAYBLUE, &sft_alb, idx_play_count);
                        }
                        // Level Meter L
                        lvl_l = (LCD_W-lm_x-1)*audio_info->lvl_l/100;
                        LCD_Fill(lm_x, lm_y, lm_x+lvl_l, lm_y+4, DARKGRAY);
                        #if defined(BOARD_SIPEED_LONGAN_NANO)
                        if (lvl_l < 80) {
                            LCD_ShowDimPictureOfs(lm_x+lvl_l, lm_y, 79, lm_y+4, 48, lm_x+lvl_l, lm_y);
                            LCD_ShowDimPictureOfs(80, lm_y, 159, lm_y+4, 48, 0, lm_y);
                        } else {
                            LCD_ShowDimPictureOfs(lm_x+lvl_l, lm_y, 159, lm_y+4, 48, lm_x+lvl_l-80, lm_y);
                        }
                        #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                        LCD_Fill(lm_x+lvl_l, lm_y, LCD_W-1, lm_y+4, BLACK);
                        #endif
                        // Level Meter R
                        lvl_r = (LCD_W-lm_x-1)*audio_info->lvl_r/100;
                        LCD_Fill(lm_x, lm_y+8, lm_x+lvl_r, lm_y+8+4, DARKGRAY);
                        #if defined(BOARD_SIPEED_LONGAN_NANO)
                        if (lvl_r < 80) {
                            LCD_ShowDimPictureOfs(lm_x+lvl_r, lm_y+8, 79, lm_y+8+4, 48, lm_x+lvl_r, lm_y+8);
                            LCD_ShowDimPictureOfs(80, lm_y+8, 159, lm_y+8+4, 48, 0, lm_y+8);
                        } else {
                            LCD_ShowDimPictureOfs(lm_x+lvl_r, lm_y+8, 159, lm_y+8+4, 48, lm_x+lvl_r-80, lm_y+8);
                        }
                        #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                        LCD_Fill(lm_x+lvl_r, lm_y+8, LCD_W-1, lm_y+8+4, BLACK);
                        #endif
                        // Progress Bar
                        if (audio_info->data_size != 0) {
                            progress = (LCD_W-pb_x-1) * (audio_info->data_offset/1024) / (audio_info->data_size/1024); // for avoid overflow
                            LCD_Fill(pb_x+progress, pb_y, LCD_W-1, pb_y+pb_h-1, DARKGRAY);
                            LCD_Fill(pb_x, pb_y, pb_x+progress, pb_y+pb_h-1, BLUE);
                        }
                        // Track Number
                        if (audio_info->number[0] != '\0') {
                            LCD_Scroll_ShowString(tn_x, tn_y, tn_x, tn_y+15, (u8 *) audio_info->number, GRAY, &sft_num, idx_play_count);
                        }
                        // Elapsed Time
                        cur_min = (audio_info->data_offset/44100/4) / 60;
                        cur_sec = (audio_info->data_offset/44100/4) % 60;
                        ttl_min = (audio_info->data_size/44100/4) / 60;
                        ttl_sec = (audio_info->data_size/44100/4) % 60;
                        if (!audio_is_pausing() || idx_play_count % 8 < 6) { // Elapsed Time blinks when pausing
                            #if defined(BOARD_SIPEED_LONGAN_NANO)
                            sprintf(lcd_str, "%3d:%02d", cur_min, cur_sec);
                            LCD_ShowString(et_x, et_y, (u8 *) lcd_str, GRAY);
                            #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                            sprintf(lcd_str, "%3d:%02d / %d:%02d", cur_min, cur_sec, ttl_min, ttl_sec);
                            LCD_ShowString(et_x, et_y, (u8 *) lcd_str, GRAY);
                            #endif
                        } else {
                            #if defined(BOARD_SIPEED_LONGAN_NANO)
                            LCD_ShowString(et_x, et_y, (u8 *) "      ", GRAY);
                            #elif defined(BOARD_LILYGO_T_DISPLAY_GD32)
                            LCD_ShowString(et_x, et_y, (u8 *) "              ", GRAY);
                            #endif
                        }
                    }
                    // Volume
                    LCD_ShowIcon(vl_x, vl_y, ICON16x16_VOLUME, 1, GRAY);
                    sprintf(lcd_str, "%3d", volume_get());
                    LCD_ShowString(vl_x+16, vl_y, (u8 *) lcd_str, GRAY);
                } else if (cover_exists && idx_play_count % 128 == 96) {
                    LCD_Clear(BLACK);
                    LCD_ShowDimPicture(ca_x, ca_y, ca_x+79, ca_y+79, 32);
                    LCD_ShowDimPicture(ca_x+80, ca_y, ca_x+80+79, ca_y+79, 32);
                    LCD_ShowPicture(ca_x+40, ca_y+0, ca_x+40+79, ca_y+79);
                /*
                } else if (cover_exists && idx_play_count % 128 >= 113 && idx_play_count % 128 < 127) {
                    // Cover Art Fade out
                    LCD_ShowDimPicture(ca_x+40, ca_y, ca_x+40+79, ca_y+79, (128 - (idx_play_count % 128))*16);
                */
                } else if (cover_exists && idx_play_count % 128 == 127) {
                    LCD_Clear(BLACK);
                    LCD_ShowDimPicture(ca_x, ca_y, ca_x+79, ca_y+79, 48);
                    LCD_ShowDimPicture(ca_x+80, ca_y, ca_x+80+79, ca_y+79, 48);
                }
                idx_play_count++;
                if (idx_idle_count < 10 * 60 * 1) {
                    set_backlight_full();
                } else if (idx_idle_count < 10 * 60 * 3) {
                    set_backlight_dark();
                } else { // Auto Power-off in 3 min
                    power_off("", 0);
                }
            } else { // if (mode == FileView)
                idx_idle_count++;
                if (idx_idle_count > 100) {
                    file_menu_idle();
                }
                if (idx_idle_count < 10 * 60 * 1) {
                    set_backlight_full();
                } else if (stack_get_count(stack) > 0 && audio_finished()) { // Random Play in 1 min if previous play finished with folder through
                    idx_random_open();
                } else if (idx_idle_count < 10 * 60 * 3) {
                    set_backlight_dark();
                } else { // Auto Power-off in 3 min
                    power_off("", 0);
                }
            }
        }
        idx_play = 0;
        // Battery
        show_battery(LCD_W-16, LCD_H-16);
        delay_1ms(100);
    }
    file_menu_close_dir();
}