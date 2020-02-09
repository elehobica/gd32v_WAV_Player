#include "lcd/lcd.h"
#include "fatfs/tf_card.h"
#include "fatfs/ff_util.h"
#include <string.h>
#include "uart_util.h"
#include "audio_buf.h"
#include "adc_util.h"

unsigned char image[12800];
int count10ms = 0;

int idx_req = 1;
int idx_head = 0;
int sort_hold_count = 0;
uint16_t max_entry_cnt;

void idx_head_up(void)
{
    if (idx_req != 1) {
        idx_req = 1;
        idx_head += 5;
        if (idx_head > max_entry_cnt - 1) idx_head = max_entry_cnt - 1;
    }
}

void idx_head_down(void)
{
    if (idx_req != 1) {
        idx_req = 1;
        idx_head -= 5;
        if (idx_head < 0) idx_head = 0;
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
            audio_pause();
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            volume_up();
            idx_head_up();
        } else if (button == HP_BUTTON_MINUS) {
            volume_down();
            idx_head_down();
        }
    } else if (button_repeat_count > 20) {
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            volume_up();
            idx_head_up();
        } else if (button == HP_BUTTON_MINUS) {
            volume_down();
            idx_head_down();
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
    char lcd_str[20];

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

    // Clear Logo
    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;

    // Search Directories / Files
    printf("Longan Player ver 1.00\n\r");
    //scan_files("", 0);

    DIR dir;
    fr = f_opendir(&dir, "");
    FILINFO fno, fno_temp;
    int target = TGT_DIRS;
    uint16_t auto_sort_max = 1;
    max_entry_cnt = idx_get_max(&dir, target, &fno);
    uint16_t *entry_list = (uint16_t *) malloc(sizeof(uint16_t) * max_entry_cnt);
    for (int i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
    uint32_t *sorted_flg = (uint32_t *) malloc(sizeof(uint32_t) * (max_entry_cnt+31)/32);
    memset(sorted_flg, 0, sizeof(uint32_t) * (max_entry_cnt+31)/32);
	char (*fast_fname_list)[FFL_SZ] = (char (*)[FFL_SZ]) malloc(sizeof(char[FFL_SZ]) * max_entry_cnt);
	for (int i = 0; i < max_entry_cnt; i++) {
		idx_f_stat(&dir, target, i, &fno);
		for (int k = 0; k < FFL_SZ; k++) {
			fast_fname_list[i][k] = fno.fname[k];
		}
		//printf("fast_fname_list[%d] = %s\n\r", i, fast_fname_list[i]);
	}

    while (1) {
        if (idx_req) {
            //idx_qsort_range_entry_list_by_lfn(&dir, target, entry_list, sorted_flg, idx_head, idx_head+5, max_entry_cnt);
            if (!get_range_full_sorted(sorted_flg, idx_head, idx_head+5)) {
                idx_qsort_entry_list_by_lfn_with_key(&dir, target, &fno, &fno_temp, entry_list, sorted_flg, fast_fname_list, idx_head, idx_head+20, 0, max_entry_cnt);
            }
            for (int i = 0; i < 5; i++) {
                memset(lcd_str, 0, 20);
                strncpy(lcd_str, "                 ", 18);
                LCD_ShowString(8*2, 16*i, (u8 *) lcd_str, GBLUE);
                if (i+idx_head < max_entry_cnt) {
                    fr = idx_f_stat(&dir, target, entry_list[i+idx_head], &fno);
                    strncpy(lcd_str, fno.fname, 17);
                    LCD_ShowString(8*2, 16*i, (u8 *) lcd_str, GBLUE);
                }
            }
            
            /*
            for (int i = 0; i < (max_entry_cnt+31)/32; i++) {
                printf("%08x\n\r", *(sorted_flg+i));
            }
            */
            
            /*
            char str[FFL_SZ+1];
            str[FFL_SZ] = '\0';
            for (int i = 0; i < max_entry_cnt; i++) {
                strncpy(str, fast_fname_list[i], FFL_SZ);
                printf("fast_fname_list[%d] = %s\n\r", i, str);
            }
            */
            //printf("\n\r");
            
            idx_req = 0;
            sort_hold_count = 0;
        } else {
            sort_hold_count++;
            if (sort_hold_count > 20 && !get_range_full_unsorted(sorted_flg, 0, max_entry_cnt)) {
                int start_next = 0;
                while (get_sorted(sorted_flg, start_next)) {
                    start_next++;
                }
                int end_1_next = start_next+1;
                while (!get_sorted(sorted_flg, end_1_next) && end_1_next < max_entry_cnt && start_next + 5 > end_1_next) {
                    end_1_next++;
                }
                idx_qsort_entry_list_by_lfn_with_key(&dir, target, &fno, &fno_temp, entry_list, sorted_flg, fast_fname_list, start_next, end_1_next, 0, max_entry_cnt);
            }
        }
        delay_1ms(100);
    }
    free(sorted_flg);
    free(entry_list);
    free(fast_fname_list);

    /*
    char entry_list[128][13];
    int32_t max_entry_cnt = sfn_make_list(&dir, entry_list, TGT_FILES, &fno);
    sfn_sort_entry_list_by_lfn(&dir, &fno, entry_list, max_entry_cnt);
    for (int i = 0; i < max_entry_cnt; i++) {
        fr = sfn_f_stat(&dir, entry_list[i], &fno);
        printf("%s\n\r", fno.fname);
    }
    */

    // Audio play
    char file_str[256];
    int file_num = 1;

    LCD_Clear(BLACK);
    BACK_COLOR=BLACK;
    audio_init();
    sprintf(file_str, "%02d.wav", file_num++);
    audio_add_playlist_wav(file_str);
    sprintf(file_str, "%02d.wav", file_num++);
    audio_play();
    while (1) {
        if (audio_add_playlist_wav(file_str)) {
            sprintf(file_str, "%02d.wav", file_num++);
            if (file_num >= 9) file_num = 1;
        }
        const audio_info_type *audio_info = audio_get_info();
        sprintf(lcd_str, "VOL %3d", volume_get());
        LCD_ShowString(8*0,  16*0, (u8 *) audio_info->filename, GBLUE);
        LCD_ShowString(8*12, 16*4, (u8 *) lcd_str, WHITE);

        //printf("%s %d/%d\n\r", audio_info.filename, audio_info.offset, audio_info.size);
        delay_1ms(100);
    }
}







