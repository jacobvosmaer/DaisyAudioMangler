#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

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

float combine(float f, float x, float y) { return f * x + (1.0 - f) * y; }
int fsign(float f) { return f > 0 ? 1 : -1; }

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
    buf.scrub = 0;
    buf.scrub_origin = hw.knob2.Process();
  }

  for (int i = 0; i < (int)size; i += 2) {
    for (int j = 0; j < 2; j++)
      buf.write[j] = in[i + j];
    buf.write = buf_wrap(buf.write + 2);
  }

  if (hw.button1.Pressed()) {
    float speed = hw.knob1.Process();
    for (int i = 0; i < (int)size; i += 2) {
      for (buf.read_frac += speed; buf.read_frac > 1.0; buf.read_frac -= 1.0)
        buf.read = buf_wrap(buf.read + buf.dir * 2);
      for (int j = 0; j < 2; j++)
        out[i + j] =
            buf.read_frac * buf.read[j] +
            (1.0 - buf.read_frac) * buf_wrap(buf.read - buf.dir * 2)[j];
    }
  } else if (hw.button2.Pressed()) {
    float new_scrub = hw.knob2.Process() - buf.scrub_origin;
    float scrub_range_seconds = 1.0;
    float sample_rate = hw.AudioSampleRate();
    float old_sample = buf.scrub * scrub_range_seconds * sample_rate;
    float new_sample = new_scrub * scrub_range_seconds * sample_rate;

    float step = (new_sample - old_sample) / (float)(size / 2);
    float sample = old_sample;
    for (int i = 0; i < (int)size; i += 2, sample += step) {
      float *read =
          buf_wrap(buf.read + 2 * fsign(sample) * (int)floorf(sample));
      float read_frac = sample - floorf(sample);
      for (int j = 0; j < 2; j++)
        out[i + j] =
            combine(read_frac, *buf_wrap(read + j), *buf_wrap(read + 2 + j));
    }

    buf.scrub = new_scrub;
  } else {
    for (int i = 0; i < (int)size; i += 2) {
      for (int j = 0; j < 2; j++)
        out[i + j] = buf.read[j];
      buf.read = buf_wrap(buf.read + 2);
    }
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
