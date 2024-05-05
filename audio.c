#include "audio.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define nchan 2
#define nelem(x) (sizeof(x) / sizeof(*(x)))

struct buf {
  float *start, *end, *write, crossfade, crossfadestep;
  int mode, oldmode;
  struct passthrough {
    float *read;
  } passthrough;
  struct varispeed {
    float *read, read_frac, speed;
    int dir;
  } varispeed;
  struct stop {
    float speed;
    int n, step;
    struct varispeed vs;
  } stop;
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

void updatevarispeed2(float *out, struct varispeed *vs) {
  buf_add(&vs->read, nchan * vs->dir * (int)floorf(vs->read_frac));
  vs->read_frac -= floorf(vs->read_frac);
  for (int j = 0; j < nchan; j++)
    out[j] = interpolate(
        vs->read_frac, vs->read[j],
        buf_wrap(vs->read - vs->dir * nchan)[j]); /* minus sounds better ?? */
  vs->read_frac += vs->speed;
}

void updatevarispeed(float *out) { updatevarispeed2(out, &buf.varispeed); }

void resetvarispeed2(struct varispeed *vs) {
  vs->read = buf.write;
  vs->read_frac = 0;
  if (!vs->dir)
    vs->dir = 1;
}

void resetvarispeed(void) { resetvarispeed2(&buf.varispeed); }

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

void resetstop(void) {
  resetvarispeed2(&buf.stop.vs);
  buf.stop.vs.speed = 1.0;
  buf.stop.n = 0;
}

void updatestop(float *out) {
  updatevarispeed2(out, &buf.stop.vs);
  if (++buf.stop.n == buf.stop.step) {
    buf.stop.n = 0;
    buf.stop.vs.speed *= buf.stop.speed;
    buf.stop.speed *= 0.999;
  }
}

/* This array must match enum order */
struct {
  void (*reset)(void);
  void (*update)(float *);
} engines[] = {
    {resetpassthrough, updatepassthrough},
    {resetvarispeed, updatevarispeed},
    {resetmute, updatemute},
    {resetstop, updatestop},
};

void buf_init(float *buffer, int size, float samplerate) {
  buf.start = buffer;
  buf.end = buffer + size;
  buf.write = buf.start;
  buf.crossfadestep = 1.0 / (0.03 * samplerate); /* 30ms crossfade */
  buf.stop.step = 0.001 * samplerate;

  for (int k = 0; k < nelem(engines); k++)
    engines[k].reset();
  memset(buffer, 0, size * sizeof(*buffer));
}

void buf_callback(const float *in, float *out, int size) {
  float frames[nelem(engines)][nchan];

  for (int i = 0; i < size; i++)
    *buf_add(&buf.write, 1) = in[i];

  for (int i = 0; i < size; i += nchan) {
    for (int k = 0; k < nelem(engines); k++)
      engines[k].update(frames[k]);
    for (int j = 0; j < nchan; j++)
      out[i + j] = interpolate(buf.crossfade, frames[buf.oldmode][j],
                               frames[buf.mode][j]);
    buf.crossfade -= buf.crossfadestep;
    if (buf.crossfade < buf.crossfadestep)
      buf.crossfade = 0;
  }
}

void buf_setdirection(int dir) { buf.varispeed.dir = dir; }

void buf_setmode(int mode) {
  if (mode == buf.mode || mode < 0 || mode >= nelem(engines))
    return;

  buf.oldmode = buf.mode;
  buf.mode = mode;
  buf.crossfade = 1.0;
  engines[buf.mode].reset();
}

void buf_setspeed(float speed) {
  buf.varispeed.speed = speed;
  float stoprange = 0.02;
  buf.stop.speed = 0.999 - stoprange + stoprange * speed;
}
