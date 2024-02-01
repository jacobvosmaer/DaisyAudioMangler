#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

enum { BUF_PASSTHROUGH, BUF_VARISPEED, BUF_MUTE, BUF_NMODES };

void buf_init(float *buffer, int size);
void buf_callback(const float *in, float *out, int size, float speed);
void buf_setdirection(int dir);
void buf_setmode(int mode);

#ifdef __cplusplus
}
#endif

#endif
