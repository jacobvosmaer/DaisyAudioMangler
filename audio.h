#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

enum { BUF_PASSTHROUGH, BUF_VARISPEED, BUF_SCRUB, BUF_MUTE };

void buf_init(float *buffer, int size, int samplerate);
void buf_callback(const float *in, float *out, int size, float speed,
                  float new_scrub);
void buf_setdirection(int dir);
void buf_setmode(int mode, float scrub_origin);
void buf_setscrubrange(float range);

#ifdef __cplusplus
}
#endif

#endif
