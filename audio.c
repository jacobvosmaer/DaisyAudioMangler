#include "audio.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define nchan 2
#define nelem(x) (sizeof(x) / sizeof(*(x)))

struct buf {
  float *start, *end, *write, crossfade;
  int dir, mode, oldmode;
  struct {
    float *read;
  } passthrough;
  struct {
    float *read, read_frac, speed;
  } varispeed;
} buf;

float *buf_wrap(float *p) {
  ptrdiff_t buf_size = buf.end - buf.start;
  if (p < buf.start)
    p += buf_size;
  else if (p >= buf.end)
    p -= buf_size;
  return p;
}

float *buf_add(float **p, ptrdiff_t n) {
  float *ret = *p;
  *p = buf_wrap(*p + n);
  return ret;
}

float interpolate(float f, float x, float y) { return f * x + (1.0 - f) * y; }

void updatevarispeed(float *out) {
  buf_add(&buf.varispeed.read,
          nchan * buf.dir * (int)floorf(buf.varispeed.read_frac));
  buf.varispeed.read_frac -= floorf(buf.varispeed.read_frac);
  for (int j = 0; j < nchan; j++)
    out[j] =
        interpolate(buf.varispeed.read_frac, buf.varispeed.read[j],
                    buf_wrap(buf.varispeed.read -
                             buf.dir * nchan)[j]); /* minus sounds better ?? */
  buf.varispeed.read_frac += buf.varispeed.speed;
}

void resetvarispeed(void) {
  buf.varispeed.read = buf.write;
  buf.varispeed.read_frac = 0;
}

void resetpassthrough(void) { buf.passthrough.read = buf.write; }

void updatepassthrough(float *out) {
  for (int j = 0; j < nchan; j++)
    out[j] = *buf_add(&buf.passthrough.read, 1);
}

void updatemute(float *out) {
  for (int j = 0; j < nchan; j++)
    out[j] = 0;
}

void resetmute(void) {}

/* This array must match enum order */
struct {
  void (*reset)(void);
  void (*update)(float *);
} engines[] = {
    {resetpassthrough, updatepassthrough},
    {resetvarispeed, updatevarispeed},
    {resetmute, updatemute},
};

void buf_init(float *buffer, int size) {
  buf.start = buffer;
  buf.end = buffer + size;
  buf.write = buf.start;
  buf.dir = 1;

  for (int k = 0; k < nelem(engines); k++)
    engines[k].reset();
  memset(buffer, 0, size * sizeof(*buffer));
}

void buf_callback(const float *in, float *out, int size) {
  float frames[BUF_NMODES][nchan];

  for (int i = 0; i < size; i++)
    *buf_add(&buf.write, 1) = in[i];

  for (int i = 0; i < size; i += nchan) {
    for (int k = 0; k < nelem(engines); k++)
      engines[k].update(frames[k]);
    for (int j = 0; j < nchan; j++)
      out[i + j] = interpolate(buf.crossfade, frames[buf.oldmode][j],
                               frames[buf.mode][j]);
    buf.crossfade -= 0.001; /* seems slow enough to stop clicks at 96kHz */
    if (buf.crossfade <= 0.1)
      buf.crossfade = 0;
  }
}

void buf_setdirection(int dir) { buf.dir = dir; }

void buf_setmode(int mode) {
  if (mode == buf.mode)
    return;

  buf.oldmode = buf.mode;
  buf.mode = mode;
  buf.crossfade = 1.0;
  engines[buf.mode].reset();
}

void buf_setspeed(float speed) { buf.varispeed.speed = speed; }
