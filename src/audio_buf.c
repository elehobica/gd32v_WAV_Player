#include "i2s_util.h"

#include <math.h>

#define SIZE_OF_SAMPLES 512  // samples for 2ch
//#define SAMPLE_RATE     (36000)
#define SAMPLE_RATE     (44100)
//#define SAMPLE_RATE     (2000)
#define WAVE_FREQ_HZ    (440)
#define PI              (3.14159265)
#define SAMPLE_PER_CYCLE (SAMPLE_RATE/WAVE_FREQ_HZ)
#define DELTA 2.0*PI*WAVE_FREQ_HZ/SAMPLE_RATE

int32_t audio_buf0[SIZE_OF_SAMPLES];
int32_t audio_buf1[SIZE_OF_SAMPLES];

static double ang = 0;
static int count = 0;
static unsigned int j = 0;
static double triangle_float = 0.0;

union U {
    uint32_t i;
    uint16_t s[2];
} u;

uint32_t swap16b(uint32_t in_val)
{
    u.i = in_val;
    return ((uint32_t) u.s[0] << 16) | ((uint32_t) u.s[1]);
}

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

void prepare_audio_buf(void)
{
    setup_triangle_sine_waves(audio_buf0);
    setup_triangle_sine_waves(audio_buf1);

    init_i2s2();
    init_dma_i2s2(audio_buf0, SIZE_OF_SAMPLES*2);

    spi_dma_enable(SPI2, SPI_DMA_TRANSMIT);
    dma_channel_enable(DMA1, DMA_CH1);
    count = 0;
}

void run_audio_buf(void)
{
    if (SET == dma_flag_get(DMA1, DMA_CH1, DMA_FLAG_FTF)) {
        dma_flag_clear(DMA1, DMA_CH1, DMA_FLAG_FTF);
        dma_channel_disable(DMA1, DMA_CH1);
        if (count % 2 == 0) {
            init_dma_i2s2(audio_buf1, SIZE_OF_SAMPLES*2);
        } else {
            init_dma_i2s2(audio_buf0, SIZE_OF_SAMPLES*2);
        }
        dma_channel_enable(DMA1, DMA_CH1);
        if (count % 2 == 0) {
           setup_triangle_sine_waves(audio_buf0);
        } else {
           setup_triangle_sine_waves(audio_buf1);
        }
        count++;
    }
}

