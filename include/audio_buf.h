#ifndef _AUDIO_BUF_H_
#define _AUDIO_BUF_H_

#include "fifo/cfifo.h"

typedef struct {
    cfifo_data_t filename;
    uint32_t size;
    uint32_t offset;
} audio_info_type;

/*
void prepare_audio_buf(char *filename);
int run_audio_buf(void);
*/
void audio_init(void);
int audio_add_playlist_wav(char *filename);
void audio_play(void);
void audio_pause(void);
int audio_is_playing(void);
const audio_info_type *audio_get_info(void);
void volume_up(void);
void volume_down(void);
int volume_get(void);

#endif
