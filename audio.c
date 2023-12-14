#ifdef __cplusplus
extern "C" {
#endif

#include "audio.h"
#include <string.h>

#define nchan 2

struct buf buf;

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

void buf_callback(const float *in, float *out, int size) {
  for (int i = 0; i < (int)size; i++)
    *buf_add(&buf.write, 1) = in[i];

  if (buf.mode == BUF_VARISPEED) {
    float speed = buf.knob1;
    for (int i = 0; i < (int)size; i += nchan, buf.read_frac += speed) {
      buf_add(&buf.read, nchan * buf.dir * (int)floorf(buf.read_frac));
      buf.read_frac -= floorf(buf.read_frac);
      for (int j = 0; j < nchan; j++)
        out[i + j] = interpolate(
            buf.read_frac, buf.read[j],
            buf_wrap(buf.read -
                     buf.dir * nchan)[j]); /* minus sounds better ?? */
    }
  } else if (buf.mode == BUF_SCRUB) {
    float new_scrub = buf.knob2 - buf.scrub_origin;
    float old_sample = buf.scrub * buf.scrub_range_s * buf.samplerate;
    float new_sample = new_scrub * buf.scrub_range_s * buf.samplerate;
    buf.scrub = new_scrub;

    float step = (new_sample - old_sample) / (float)(size / nchan);
    float sample = old_sample;
    for (int i = 0; i < (int)size; i += nchan, sample += step) {
      float *read = buf_wrap(buf.read + nchan * (int)floorf(sample));
      float read_frac = sample - floorf(sample);
      for (int j = 0; j < nchan; j++)
        out[i + j] = interpolate(read_frac, read[j], buf_wrap(read + nchan)[j]);
    }
  } else { /* normal playback */
    for (int i = 0; i < (int)size; i++)
      out[i] = *buf_add(&buf.read, 1);
  }
}

#ifdef __cplusplus
}
#endif
