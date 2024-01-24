#include "audio.h"
#include <string.h>

#define nchan 2

struct buf {
  float *start, *end, *write, *read, read_frac, scrub, scrub_origin,
      scrub_range_s;
  int dir, mode, samplerate;
  float knob1, knob2;
} buf;

void buf_init(float *buffer, int size, int samplerate) {
  buf.start = buffer;
  buf.end = buffer + size;
  buf.write = buf.start;
  buf.read = buf.start;
  buf.read_frac = 0;
  buf.scrub_range_s = 1.0;
  buf.dir = 1;
  buf.samplerate = samplerate;

  memset(buffer, 0, size * sizeof(*buffer));
}

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

void buf_callback(const float *in, float *out, int size, float speed,
                  float new_scrub) {
  for (int i = 0; i < size; i++)
    *buf_add(&buf.write, 1) = in[i];

  if (buf.mode == BUF_VARISPEED) {
    for (int i = 0; i < size; i += nchan, buf.read_frac += speed) {
      buf_add(&buf.read, nchan * buf.dir * (int)floorf(buf.read_frac));
      buf.read_frac -= floorf(buf.read_frac);
      for (int j = 0; j < nchan; j++)
        out[i + j] = interpolate(
            buf.read_frac, buf.read[j],
            buf_wrap(buf.read -
                     buf.dir * nchan)[j]); /* minus sounds better ?? */
    }
  } else if (buf.mode == BUF_SCRUB) {
    new_scrub -= buf.scrub_origin;
    float old_sample = buf.scrub * buf.scrub_range_s * buf.samplerate;
    float new_sample = new_scrub * buf.scrub_range_s * buf.samplerate;
    buf.scrub = new_scrub;

    float step = (new_sample - old_sample) / (float)(size / nchan);
    float sample = old_sample;
    for (int i = 0; i < size; i += nchan, sample += step) {
      float *read = buf_wrap(buf.read + nchan * (int)floorf(sample));
      float read_frac = sample - floorf(sample);
      for (int j = 0; j < nchan; j++)
        out[i + j] = interpolate(read_frac, read[j], buf_wrap(read + nchan)[j]);
    }
  } else if (buf.mode == BUF_PASSTHROUGH) {
    for (int i = 0; i < size; i++)
      out[i] = *buf_add(&buf.read, 1);
  } else if (buf.mode == BUF_MUTE) {
    memset(out, 0, sizeof(*out) * size);
  }
}

void buf_setdirection(int dir) { buf.dir = dir; }
void buf_setscrubrange(float range) { buf.scrub_range_s = range; }

void buf_setmode(int mode, float scrub_origin) {
  buf.mode = mode;
  if (mode == BUF_PASSTHROUGH) {
    /* We are returning to normal playback but buf.read is possibly
     * desynchronized from buf.write. */
    buf.read = buf.write;
    buf.read_frac = 0;
  } else if (mode == BUF_SCRUB) {
    buf.scrub_origin = scrub_origin;
    buf.scrub = 0;
  }
}
