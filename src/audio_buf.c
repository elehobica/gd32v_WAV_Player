#include "fatfs/tf_card.h"
#include "i2s_util.h"
#include "audio_buf.h"

#define SIZE_OF_SAMPLES (1024)  // samples for 2ch
//#define SAMPLE_RATE     (36000)
#define SAMPLE_RATE     (44100)
//#define SAMPLE_RATE     (2000)

#define INIT_MUTE_COUNT 25

// Audio Double Buffer from DMA transfer
int32_t audio_buf[2][SIZE_OF_SAMPLES];
// Audio Buffer for File Read
int16_t buf_16b[SIZE_OF_SAMPLES];

static int count = 0;

static cfifo_t *playlist;
FIL fil;
audio_info_type audio_info;
int32_t dma_trans_number;
int next_is_end = 0;
int playing = 0;
int pausing = 0;

static int volume = 65; // 0 ~ 100;
// vol_table[34] = 256;
static const uint32_t vol_table[] = {0, 4, 8, 12, 16, 20, 24, 27, 29, 31, 34, 37, 40, 44, 48, 52, 57, 61, 67, 73, 79, 86, 94, 102, 111, 120, 131, 142, 155, 168, 183, 199, 217, 236, 256, 279, 303, 330, 359, 390, 424, 462, 502, 546, 594, 646, 703, 764, 831, 904, 983, 1069, 1163, 1265, 1376, 1496, 1627, 1770, 1925, 2094, 2277, 2476, 2693, 2929, 3186, 3465, 3769, 4099, 4458, 4849, 5274, 5736, 6239, 6785, 7380, 8026, 8730, 9495, 10327, 11232, 12216, 13286, 14450, 15716, 17093, 18591, 20220, 21992, 23919, 26015, 28294, 30773, 33470, 36403, 39592, 43061, 46835, 50938, 55402, 60256, 65536};

static uint32_t swap16b(uint32_t in_val)
{
    union U {
        uint32_t i;
        uint16_t s[2];
    } u;
    u.i = in_val;
    return ((uint32_t) u.s[0] << 16) | ((uint32_t) u.s[1]);
}

static int load_next_file(void)
{
    FRESULT fr;     /* FatFs return code */
    UINT br;

    //PA_OUT(3, 1);
    if (!cfifo_read(playlist, audio_info.filename)) {
        return 0;
    }

    fr = f_open(&fil, audio_info.filename, FA_READ);
    if (fr != FR_OK) {
        printf("ERROR: f_open %d\n\r", (int) fr);
    }
    audio_info.offset = 40;

    f_lseek(&fil, audio_info.offset);
    fr = f_read(&fil, &audio_info.size, sizeof(audio_info.size), &br);
    //printf("Play \"%s\" Size: %d\n\r", (char *) audio_info.filename, (int) audio_info.size);
    audio_info.offset = 44;
    //f_lseek(&fil, audio_info.offset); /* lseek is not needed because f_read automatically make position go forward */
    //PA_OUT(3, 0);
    return 1;
}

static int get_audio_buf(FIL *tec, int32_t *buf_32b, int32_t *trans_number)
// trans_number: DMA transfer count of 16bit->32bit transfer (NOT Byte count)
// but it equals Byte count of 16bit RAW data (actually equals (Byte count of 16bit RAW data)*2/2)
// because 16bit RAW data is expanded into 32bit data for 24bit DAC
{
    int i;
    FRESULT fr;     /* FatFs return code */
    UINT br;
    int next_is_end = 0; /* 0: continue, 1: next is end */

    if (count < INIT_MUTE_COUNT || pausing) {
        for (i = 0; i < SIZE_OF_SAMPLES; i++) buf_32b[i] = 0;
        *trans_number = SIZE_OF_SAMPLES*2;
        return 0;
    }
    if (count == INIT_MUTE_COUNT) {
        load_next_file();
    }

    uint32_t number = 0; // number to transfer
    while (number < sizeof(buf_16b)) {
        uint32_t file_rest = 44 + audio_info.size - audio_info.offset;
        uint32_t trans_rest = sizeof(buf_16b) - number;
        uint32_t trans = (file_rest >= trans_rest) ? trans_rest : file_rest;
        //LEDR(1);
        fr = f_read(&fil, &buf_16b[number/2], trans, &br);
        //LEDR(0);
        if (fr == FR_OK) {
            number += trans;
            audio_info.offset += trans;
            //f_lseek(&fil, audio_info.offset); /* lseek is not needed because f_read automatically make position go forward */
        } else {
            printf("ERROR: f_read %d\n\r", (int) fr);
            f_close(&fil);
            *trans_number = number;
            return 1;
        }
        if (44 + audio_info.size <= audio_info.offset) {
            f_close(&fil);
            if (!load_next_file()) { // End of playlist
                next_is_end = 1;
                break;
            }
        }
    }
    for (i = 0; i < number/4; i++) {
        buf_32b[i*2+0] = swap16b((int) buf_16b[i*2+0] * vol_table[volume]); // L
        buf_32b[i*2+1] = swap16b((int) buf_16b[i*2+1] * vol_table[volume]); // R
    }
    *trans_number = number;
    return next_is_end;
}

void audio_init(void)
{
    playlist = cfifo_create(2);
    count = 0;
    playing = 0;
    pausing = 0;
}

int audio_add_playlist_wav(char *filename)
{
    return cfifo_write(playlist, filename);
}

void audio_play(void)
{
    if (playing) return;
    if (cfifo_is_empty(playlist)) return;

    for (int i = 0; i < SIZE_OF_SAMPLES; i++) {
        audio_buf[0][i] = 0;
        audio_buf[1][i] = 0;
    }
    dma_trans_number = SIZE_OF_SAMPLES*2;

    init_i2s2();
    spi_dma_enable(SPI2, SPI_DMA_TRANSMIT);
    init_dma_i2s2(audio_buf[0], dma_trans_number);
    dma_channel_enable(DMA1, DMA_CH1);
    dma_interrupt_enable(DMA1, DMA_CH1, DMA_INT_FTF);
    eclic_irq_enable(DMA1_Channel1_IRQn, 15, 15); // level = 15, priority = 15 (MAX)

    count = 0;
    playing = 1;
    pausing = 0;
    next_is_end = 0;
}

void audio_pause(void)
{
    if (!playing) return;
    pausing = 1 - pausing;
}

void DMA1_Channel1_IRQHandler(void)
{
    int nxt1 = (count & 0x1) ^ 0x1;
    int nxt2 = 1 - nxt1;
    dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);
    dma_channel_disable(DMA1, DMA_CH1);
    if (next_is_end) {
        playing = 0;
        return;
    }
    LEDB(0);
    init_dma_i2s2(audio_buf[nxt1], dma_trans_number);
    dma_channel_enable(DMA1, DMA_CH1);
    next_is_end = get_audio_buf(&fil, audio_buf[nxt2], &dma_trans_number);
    count++;
    LEDB(1);
    //dma_interrupt_flag_clear(DMA1, DMA_CH1, DMA_INT_FLAG_G);  /* not needed */
}

int audio_is_playing(void)
{
    return playing;
}

const audio_info_type *audio_get_info(void)
{
    return (const audio_info_type *) &audio_info;
}

void volume_up(void)
{
    if (volume < 100) volume++;
}

void volume_down(void)
{
    if (volume > 0) volume--;
}

int volume_get(void)
{
    return volume;
}

