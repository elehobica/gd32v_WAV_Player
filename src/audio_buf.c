#include "i2s_util.h"
#include "fatfs/tf_card.h"

#include <math.h>

#define SIZE_OF_SAMPLES 512  // samples for 2ch
//#define SAMPLE_RATE     (36000)
#define SAMPLE_RATE     (44100)
//#define SAMPLE_RATE     (2000)

int32_t audio_buf0[SIZE_OF_SAMPLES];
int32_t audio_buf1[SIZE_OF_SAMPLES];

static int count = 0;

union U {
    uint32_t i;
    uint16_t s[2];
} u;

FIL fil;
int offset;
int wav_data_size;
int32_t dma_trans_number;
int buf_flg = 0;

uint32_t swap16b(uint32_t in_val)
{
    u.i = in_val;
    return ((uint32_t) u.s[0] << 16) | ((uint32_t) u.s[1]);
}

int volume = 65; // 0 ~ 100;
static const uint32_t vol_table[] = {0, 4, 8, 12, 16, 20, 24, 27, 29, 31, 34, 37, 40, 44, 48, 52, 57, 61, 67, 73, 79, 86, 94, 102, 111, 120, 131, 142, 155, 168, 183, 199, 217, 236, 256, 279, 303, 330, 359, 390, 424, 462, 502, 546, 594, 646, 703, 764, 831, 904, 983, 1069, 1163, 1265, 1376, 1496, 1627, 1770, 1925, 2094, 2277, 2476, 2693, 2929, 3186, 3465, 3769, 4099, 4458, 4849, 5274, 5736, 6239, 6785, 7380, 8026, 8730, 9495, 10327, 11232, 12216, 13286, 14450, 15716, 17093, 18591, 20220, 21992, 23919, 26015, 28294, 30773, 33470, 36403, 39592, 43061, 46835, 50938, 55402, 60256, 65536};

/*
static double ang = 0;
static unsigned int j = 0;
static double triangle_float = 0.0;

#define WAVE_FREQ_HZ    (440)
#define PI              (3.14159265)
#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)
#define DELTA 2.0*PI*WAVE_FREQ_HZ/SAMPLE_RATE

double _square_wave(void)
{
    double dval;
    if (ang >= 2.0*PI) {
        ang -= 2.0*PI;
        triangle_float = -(double) pow(2, 22);
    }
    if (ang < PI) {
        dval = 1.0;
    } else {
        dval = -1.0;
    }
    return dval;
}

static void setup_triangle_sine_waves(int32_t *samples_data)
{
    unsigned int i;
    double square_float;
    double triangle_step = (double) pow(2, 23) / SAMPLE_PER_CYCLE;

    for(i = 0; i < SIZE_OF_SAMPLES/2; i++) {
        square_float = _square_wave();
        ang += DELTA;
        j++;
        if (square_float >= 0) {
            triangle_float += triangle_step;
        } else {
            triangle_float -= triangle_step;
        }

        square_float *= (pow(2, 23) - 1);
        samples_data[i*2+0] = swap16b((int) square_float * 256);
        samples_data[i*2+1] = swap16b((int) triangle_float * 256);
    }
}
*/

int get_audio_buf(FIL *tec, int32_t *samples_data, int32_t *trans_number)
// trans_number: DMA transfer count of 16bit->32bit transfer (NOT Byte count)
// but it equals Byte count of 16bit RAW data (actually equals (Byte count of 16bit RAW data)*2/2)
// because 16bit RAW data is expanded into 32bit data for 24bit DAC
{
    int16_t audio_buf[SIZE_OF_SAMPLES];
    FRESULT fr;     /* FatFs return code */
    UINT br;
    unsigned int i;
    f_lseek(&fil, offset);
    if (offset + sizeof(audio_buf) > 44 + wav_data_size) {
        *trans_number = 44 + wav_data_size - offset; // *2/2
    } else {
        *trans_number = sizeof(audio_buf); // *2/2
    }
    fr = f_read(&fil, audio_buf, *trans_number, &br);
    if (fr == 0) {
        //printf("OK %d\n\r", offset);
        for (i = 0; i < *trans_number/4; i++) {
            samples_data[i*2+0] = swap16b((int) audio_buf[i*2+0]* vol_table[volume]);
            samples_data[i*2+1] = swap16b((int) audio_buf[i*2+1]* vol_table[volume]);
        }
    } else {
        printf("NG %d %d\n\4", offset, (int) *trans_number);
    }
    offset += sizeof(audio_buf);
    return (offset >= 44 + wav_data_size);
}

void prepare_audio_buf(char *filename)
{
    FRESULT fr;     /* FatFs return code */
    UINT br;

    fr = f_open(&fil, filename, FA_READ);
    if (fr) printf("open error: %d!\n\r", (int)fr);
    offset = 40;
    f_lseek(&fil, offset);
    fr = f_read(&fil, &wav_data_size, sizeof(wav_data_size), &br);
    printf("data size: %d\n\r", wav_data_size);
    offset = 44;

    for (int i = 0; i < SIZE_OF_SAMPLES; i++) {
        audio_buf0[i] = 0;
        audio_buf1[i] = 0;
    }
    dma_trans_number = SIZE_OF_SAMPLES*2;
    buf_flg = 0;
    
    //buf_flg = get_audio_buf(&fil, audio_buf0, &dma_trans_number);

    init_i2s2();
    spi_dma_enable(SPI2, SPI_DMA_TRANSMIT);
    init_dma_i2s2(audio_buf0, dma_trans_number);
    dma_channel_enable(DMA1, DMA_CH1);

    //buf_flg = get_audio_buf(&fil, audio_buf1, &dma_trans_number);
    count = 0;
}

int run_audio_buf(void)
{
    if (SET == dma_flag_get(DMA1, DMA_CH1, DMA_FLAG_FTF)) {
        LEDB(0);
        dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);
        dma_channel_disable(DMA1, DMA_CH1);
        if (count % 2 == 0) {
            init_dma_i2s2(audio_buf1, dma_trans_number);
        } else {
            init_dma_i2s2(audio_buf0, dma_trans_number);
        }
        dma_channel_enable(DMA1, DMA_CH1);
        if (buf_flg) {
            f_close(&fil);
            return 0;
        }
        if (count > 50) {
            if (count % 2 == 0) {
                buf_flg = get_audio_buf(&fil, audio_buf0, &dma_trans_number);
            } else {
                buf_flg = get_audio_buf(&fil, audio_buf1, &dma_trans_number);
            }
        }
        count++;
        LEDB(1);
    }
    return 1;
}

