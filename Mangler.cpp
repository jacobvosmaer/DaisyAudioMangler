#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))
#define nchan 2

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

struct {
  float *start, *end, *write, *read, read_frac, scrub, scrub_origin;
  int dir;
} buf;

void buf_init(void) {
  buf.start = buffer;
  buf.end = buffer + nelem(buffer);
  buf.write = buf.start;
  buf.read = buf.start;
  buf.read_frac = 0;
  buf.dir = 1;

  memset(buffer, 0, sizeof(buffer));
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

float combine(float f, float x, float y) { return f * x + (1.0 - f) * y; }

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  hw.ProcessAllControls();

  int new_dir = hw.encoder.Increment();
  if (new_dir)
    buf.dir = new_dir;

  if (hw.button1.FallingEdge() || hw.button2.FallingEdge()) {
    /* We are returning to normal playback but buf.read is possibly
     * desynchronized from buf.write. */
    buf.read = buf.write;
    buf.read_frac = 0;
  }

  if (hw.button2.RisingEdge()) { /* start scrubbing mode */
    /* We want to scrub relative to the current position of knob2. */
    buf.scrub_origin = hw.knob2.Process();
    buf.scrub = 0;
  }

  for (int i = 0; i < (int)size; i++)
    *buf_add(&buf.write, 1) = in[i];

  if (hw.button1.Pressed()) { /* variable speed playback */
    float speed = hw.knob1.Process();
    for (int i = 0; i < (int)size; i += nchan, buf.read_frac += speed) {
      buf_add(&buf.read, nchan * buf.dir * (int)floorf(buf.read_frac));
      buf.read_frac -= floorf(buf.read_frac);
      for (int j = 0; j < nchan; j++)
        out[i + j] = combine(buf.read_frac, buf.read[j],
                             buf_wrap(buf.read - buf.dir * nchan)[j]);
    }
  } else if (hw.button2.Pressed()) { /* scrub mode */
    float new_scrub = hw.knob2.Process() - buf.scrub_origin;
    float scrub_range_s = 1.0;
    float old_sample = buf.scrub * scrub_range_s * hw.AudioSampleRate();
    float new_sample = new_scrub * scrub_range_s * hw.AudioSampleRate();
    buf.scrub = new_scrub;

    float step = (new_sample - old_sample) / (float)(size / nchan);
    float sample = old_sample;
    for (int i = 0; i < (int)size; i += nchan, sample += step) {
      float *read = buf_wrap(buf.read + nchan * (int)floorf(sample));
      float read_frac = sample - floorf(sample);
      for (int j = 0; j < nchan; j++)
        out[i + j] = combine(read_frac, read[j], buf_wrap(read + nchan)[j]);
    }
  } else { /* normal playback */
    for (int i = 0; i < (int)size; i++)
      out[i] = *buf_add(&buf.read, 1);
  }
}

int main(void) {
  hw.Init();
  buf_init();
  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
