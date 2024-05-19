#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

enum buf_mode {
  BUF_PASSTHROUGH,
  BUF_VARISPEED,
  BUF_MUTE,
  BUF_STOP,
  BUF_SAMPLEPLAY
};

void buf_init(float *buffer, int size, float samplerate);
void buf_callback(const float *in, float *out, int size);
void buf_setdirection(int dir);
void buf_setmode(enum buf_mode mode);
void buf_setspeed(float speed);
void buf_sample_start(void);
void buf_sample_stop(void);
void buf_sample_trigger(float speed);

#ifdef __cplusplus
}
#endif

#endif
