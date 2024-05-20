#include "audio.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define nchan 2
#define nelem(x) (sizeof(x) / sizeof(*(x)))

typedef float frame[nchan];

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
  struct sample {
    float *start, *end;
    struct varispeed vs;
  } sample;
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

void updatevarispeed2(frame out, struct varispeed *vs) {
  buf_add(&vs->read, nchan * vs->dir * (int)floorf(vs->read_frac));
  vs->read_frac -= floorf(vs->read_frac);
  for (int j = 0; j < nchan; j++) {
    /* Cubic interpolation from PD tabread4~. We interpolate a sample in between
     * b and c. We need a and d to approximate the derivative of the waveform at
     * b and c. (I think that is what is happening.) */
    float *read = buf_wrap(vs->read + j), a = *buf_wrap(read - nchan),
          b = *read, c = *buf_wrap(read + nchan),
          d = *buf_wrap(read + 2 * nchan), cminusb = c - b;
    out[j] = b + vs->read_frac *
                     (cminusb - 0.1666667f * (1.0f - vs->read_frac) *
                                    ((d - a - 3.0f * cminusb) * vs->read_frac +
                                     (d + 2.0f * a - 3.0f * b)));
  }
  vs->read_frac += vs->speed;
}

void updatevarispeed(frame out) { updatevarispeed2(out, &buf.varispeed); }

float *read_init(void) { return buf_wrap(buf.write - nchan); }

void resetvarispeed2(struct varispeed *vs) {
  vs->read = read_init();
  vs->read_frac = 0;
  if (!vs->dir)
    vs->dir = 1;
}

void resetvarispeed(void) { resetvarispeed2(&buf.varispeed); }

void resetpassthrough(void) { buf.passthrough.read = read_init(); }

void updatepassthrough(frame out) {
  for (int j = 0; j < nchan; j++)
    out[j] = *buf_add(&buf.passthrough.read, 1);
}

void updatemute(frame out) {
  for (int j = 0; j < nchan; j++)
    out[j] = 0;
}

void resetmute(void) {}

void resetstop(void) {
  resetvarispeed2(&buf.stop.vs);
  buf.stop.vs.speed = 1.0;
  buf.stop.n = 0;
}

void updatestop(frame out) {
  updatevarispeed2(out, &buf.stop.vs);
  if (++buf.stop.n == buf.stop.step) {
    buf.stop.n = 0;
    buf.stop.vs.speed *= buf.stop.speed;
    buf.stop.speed *= 0.999;
  }
}

void resetsample(void) {
  resetvarispeed2(&buf.sample.vs);
  buf.sample.vs.read = buf.sample.start;
}

void updatesample(frame out) {
  struct varispeed *vs = &buf.sample.vs;
  float *start = buf.sample.start, *end = buf.sample.end, *read = vs->read;
  if ((start < end && read >= start && read < end) ||
      (end < start && (read >= start || read < end)))
    updatevarispeed2(out, vs);
  else
    updatemute(out);
}

/* This array must match enum buf_mode order */
struct {
  void (*reset)(void);
  void (*update)(frame);
} engines[] = {
    {resetpassthrough, updatepassthrough},
    {resetvarispeed, updatevarispeed},
    {resetmute, updatemute},
    {resetstop, updatestop},
    {resetsample, updatesample},
};

void buf_init(float *buffer, int size, float samplerate) {
  buf.start = buffer;
  buf.end = buffer + size;
  buf.write = buf.start;
  buf.crossfadestep = 1.0 / (0.03 * samplerate); /* 30ms crossfade */
  buf.stop.step = 0.001 * samplerate;
  buf.sample.start = buf.sample.end = buf.write;

  for (int k = 0; k < nelem(engines); k++)
    engines[k].reset();
  memset(buffer, 0, size * sizeof(*buffer));
}

void buf_callback(const float *in, float *out, int size) {
  frame frames[nelem(engines)];

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

void buf_setmode(enum buf_mode mode) {
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

int signf(float f) {
  float epsilon = 0.0001;
  if (f > -epsilon && f < epsilon)
    return 0;
  else if (f > 0)
    return 1;
  else
    return -1;
}

float *zero_crossing(void) {
  float *f = buf.write;
  int oldsign, sign = signf(*f);
  if (!sign)
    return f;

  oldsign = sign;
  while (sign == oldsign) {
    buf_add(&f, -nchan);
    sign = signf(*f);
  }

  return f;
}

void buf_sample_start(void) { buf.sample.start = zero_crossing(); }
void buf_sample_stop(void) { buf.sample.end = zero_crossing(); }
void buf_sample_trigger(float speed) {
  buf.sample.vs.speed = speed;
  if (buf.mode == BUF_SAMPLEPLAY)
    buf.mode = BUF_MUTE;
  buf_setmode(BUF_SAMPLEPLAY);
  buf.crossfade = 0;
}
