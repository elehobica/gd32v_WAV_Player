#include "fatfs/tf_card.h"
#include "i2s_util.h"
#include "audio_buf.h"

#define SIZE_OF_SAMPLES 512  // samples for 2ch
//#define SAMPLE_RATE     (36000)
#define SAMPLE_RATE     (44100)
//#define SAMPLE_RATE     (2000)

// Audio Double Buffer from DMA transfer
int32_t audio_buf[2][SIZE_OF_SAMPLES];

static int count = 0;

static cfifo_t *playlist;
FIL fil;
audio_info_type audio_info;
int32_t dma_trans_number;
int next_is_end = 0;
int playing = 0;
int pausing = 0;

static int volume = 65; // 0 ~ 100;
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

static void load_next_file(void)
{
    FRESULT fr;     /* FatFs return code */
    UINT br;

    cfifo_read(playlist, audio_info.filename);

    fr = f_open(&fil, audio_info.filename, FA_READ);
    if (fr) {
        printf("open error: %d!\n\r", (int) fr);
    }
    audio_info.offset = 40;

    f_lseek(&fil, audio_info.offset);
    fr = f_read(&fil, &audio_info.size, sizeof(audio_info.size), &br);
    printf("Play \"%s\" Size: %d\n\r", (char *) audio_info.filename, (int) audio_info.size);
    audio_info.offset = 44;
}

static int get_audio_buf(FIL *tec, int32_t *buf_32b, int32_t *trans_number)
// trans_number: DMA transfer count of 16bit->32bit transfer (NOT Byte count)
// but it equals Byte count of 16bit RAW data (actually equals (Byte count of 16bit RAW data)*2/2)
// because 16bit RAW data is expanded into 32bit data for 24bit DAC
{
    int16_t buf_16b[SIZE_OF_SAMPLES];
    FRESULT fr;     /* FatFs return code */
    UINT br;
    unsigned int i;
    //f_lseek(&fil, audio_info.offset); /* not needed because f_read automatically make position go forward */
    if (audio_info.offset + sizeof(buf_16b) > 44 + audio_info.size) {
        *trans_number = 44 + audio_info.size - audio_info.offset; // *2/2
    } else {
        *trans_number = sizeof(buf_16b); // *2/2
    }
    fr = f_read(&fil, buf_16b, *trans_number, &br);
    if (fr == 0) {
        //printf("OK %d\n\r", audio_info.offset);
        for (i = 0; i < *trans_number/4; i++) {
            buf_32b[i*2+0] = swap16b((int) buf_16b[i*2+0]* vol_table[volume]);
            buf_32b[i*2+1] = swap16b((int) buf_16b[i*2+1]* vol_table[volume]);
        }
    } else {
        printf("NG %d %d\n\4", (int) audio_info.offset, (int) *trans_number);
    }
    audio_info.offset += sizeof(buf_16b);
    return (audio_info.offset >= 44 + audio_info.size);
}

void audio_init(void)
{
    playlist = cfifo_create(5);
    playing = 0;
}

int audio_add_playlist_wav(char *filename)
{
    return cfifo_write(playlist, filename);
}

void audio_play(void)
{
    if (playing) return;
    if (cfifo_is_empty(playlist)) return;

    load_next_file();

    for (int i = 0; i < SIZE_OF_SAMPLES; i++) {
        audio_buf[0][i] = 0;
        audio_buf[1][i] = 0;
    }
    dma_trans_number = SIZE_OF_SAMPLES*2;
    next_is_end = 0;

    init_i2s2();
    spi_dma_enable(SPI2, SPI_DMA_TRANSMIT);
    init_dma_i2s2(audio_buf[0], dma_trans_number);
    dma_channel_enable(DMA1, DMA_CH1);
    dma_interrupt_enable(DMA1, DMA_CH1, DMA_INT_FTF);
    eclic_irq_enable(DMA1_Channel1_IRQn, 15, 15); // level = 15, priority = 15 (MAX)

    count = 0;
    playing = 1;
    pausing = 0;
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
        f_close(&fil);
        if (cfifo_is_empty(playlist)) {
            playing = 0;
            return;
        } else {
            load_next_file();
        }
    }
    LEDB(0);
    init_dma_i2s2(audio_buf[nxt1], dma_trans_number);
    dma_channel_enable(DMA1, DMA_CH1);
    if (count > 50) {
        if (pausing) {
            for (int i = 0; i < SIZE_OF_SAMPLES; i++) audio_buf[nxt2][i] = 0;
        } else {
            next_is_end = get_audio_buf(&fil, audio_buf[nxt2], &dma_trans_number);
        }
    }
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

