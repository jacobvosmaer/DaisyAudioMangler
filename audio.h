#ifndef AUDIO_H
#define AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

extern struct buf {
  float *start, *end, *write, *read, read_frac, scrub, scrub_origin,
      scrub_range_s;
  int dir, mode, samplerate;
  float knob1, knob2;
} buf;
enum { BUF_PASSTHROUGH, BUF_VARISPEED, BUF_SCRUB };

void buf_init(float *buffer, int size, int samplerate);
void buf_callback(const float *in, float *out, int size);

#ifdef __cplusplus
}
#endif

#endif
