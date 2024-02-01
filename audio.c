#include "audio.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define nchan 2

struct buf {
  float *start, *end, *write, *read, read_frac;
  int dir, mode;
  float knob1, knob2;
  struct {
    float *read, read_frac, speed;
  } varispeed;
} buf;

void buf_init(float *buffer, int size) {
  buf.start = buffer;
  buf.end = buffer + size;
  buf.write = buf.start;
  buf.read = buf.start;
  buf.read_frac = 0;
  buf.varispeed.read = buf.start;
  buf.varispeed.read_frac = 0;
  buf.dir = 1;

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

void updatepassthrough(float *out) {
  for (int j = 0; j < nchan; j++)
    out[j] = *buf_add(&buf.read, 1);
}

void updatemute(float *out) { out[0] = out[1] = 0; }

void buf_callback(const float *in, float *out, int size, float speed) {
  //  float frames[BUF_NMODES][nchan];

  for (int i = 0; i < size; i++)
    *buf_add(&buf.write, 1) = in[i];
  buf.varispeed.speed = speed;

  if (buf.mode == BUF_VARISPEED) {
    for (int i = 0; i < size; i += nchan) {
      updatevarispeed(out + i);
    }
  } else if (buf.mode == BUF_PASSTHROUGH) {
    for (int i = 0; i < size; i += nchan) {
      updatepassthrough(out + i);
    }
  } else if (buf.mode == BUF_MUTE) {
    for (int i = 0; i < size; i += nchan) {
      updatemute(out + i);
    }
  }
}

void buf_setdirection(int dir) { buf.dir = dir; }

void buf_setmode(int mode) {
  buf.mode = mode;
  if (mode == BUF_PASSTHROUGH) {
    /* We are returning to normal playback but buf.read is possibly
     * desynchronized from buf.write. */
    buf.read = buf.write;
    buf.read_frac = 0;
  }
}
