#ifndef _AUDIO_BUF_H_
#define _AUDIO_BUF_H_

typedef struct {
    char filename[256];
    uint32_t size;
    uint32_t data_start;
    uint32_t offset;
    char artist[256];
    char title[256];
    char album[256];
} audio_info_type;


void audio_init(void);
void audio_play(uint16_t idx);
void audio_pause(void);
void audio_stop(void);
int audio_is_playing_or_pausing(void);
const audio_info_type *audio_get_info(void);
void volume_up(void);
void volume_down(void);
int volume_get(void);

#endif
