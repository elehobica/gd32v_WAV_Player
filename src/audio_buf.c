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
int data_size;
int next_dma_size;
int buf_flg = 0;

uint32_t swap16b(uint32_t in_val)
{
    u.i = in_val;
    return ((uint32_t) u.s[0] << 16) | ((uint32_t) u.s[1]);
}

int volume = 2000; // 256 ~ 65536

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

int get_audio_buf(FIL *tec, int32_t *samples_data, int32_t *dma_size)
{
    int16_t audio_buf[SIZE_OF_SAMPLES];
    FRESULT fr;     /* FatFs return code */
    UINT br;
    unsigned int i;
    f_lseek(&fil, offset);
    if (offset + sizeof(audio_buf) > 44 + data_size) {
        *dma_size = 44 + data_size - offset;
    } else {
        *dma_size = sizeof(audio_buf);
    }
    fr = f_read(&fil, audio_buf, *dma_size, &br);
    if (fr == 0) {
        //printf("OK %d\n\r", offset);
        for (i = 0; i < *dma_size/4; i++) {
            samples_data[i*2+0] = swap16b((int) audio_buf[i*2+0]* volume);
            samples_data[i*2+1] = swap16b((int) audio_buf[i*2+1]* volume);
        }
    } else {
        printf("NG %d %d\n\4", offset, *dma_size);
    }
    offset += sizeof(audio_buf);
    return (offset >= 44 + data_size);
}

void prepare_audio_buf(void)
{
    FRESULT fr;     /* FatFs return code */
    UINT br;
    int flg = 0;

    //fr = f_open(&fil, "bmp.bin", FA_READ);
    fr = f_open(&fil, "alone.wav", FA_READ);
    if (fr) printf("open error: %d!\n\r", (int)fr);
    offset = 40;
    f_lseek(&fil, offset);
    fr = f_read(&fil, &data_size, sizeof(data_size), &br);
    printf("data size: %d\n\r", data_size);
    offset = 44;

    buf_flg = get_audio_buf(&fil, audio_buf0, &next_dma_size);


    init_i2s2();
    init_dma_i2s2(audio_buf0, next_dma_size);

    spi_dma_enable(SPI2, SPI_DMA_TRANSMIT);
    dma_channel_enable(DMA1, DMA_CH1);

    buf_flg = get_audio_buf(&fil, audio_buf1, &next_dma_size);
    count = 0;
}

int run_audio_buf(void)
{
    if (SET == dma_flag_get(DMA1, DMA_CH1, DMA_FLAG_FTF)) {
        dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);
        dma_channel_disable(DMA1, DMA_CH1);
        if (count % 2 == 0) {
            init_dma_i2s2(audio_buf1, next_dma_size);
        } else {
            init_dma_i2s2(audio_buf0, next_dma_size);
        }
        dma_channel_enable(DMA1, DMA_CH1);
        if (buf_flg) {
            f_close(&fil);
            return 0;
        }
        if (count % 2 == 0) {
            buf_flg = get_audio_buf(&fil, audio_buf0, &next_dma_size);
        } else {
            buf_flg = get_audio_buf(&fil, audio_buf1, &next_dma_size);
        }
        count++;
    }
    return 1;
}

