#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

struct {
  float *start, *end, *write, *read, read_frac, last_knob2, speed;
  int dir;
  uint32_t last_knob2_update;
} buf;

void buf_init(void) {
  buf.start = buffer;
  buf.end = buffer + nelem(buffer);
  buf.write = buf.start;
  buf.read = buf.start;
  buf.read_frac = 0;
  buf.last_knob2 = 0;
  buf.speed = 1;
  buf.dir = 1;
  buf.last_knob2_update = 0;
}

float *bufWrap(float *p) {
  ptrdiff_t buf_size = buf.end - buf.start;
  if (p < buf.start)
    p += buf_size;
  else if (p >= buf.end)
    p -= buf_size;
  return p;
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  float *old_write = buf.write;
  float speed = 1.0;

  /* Read input buffer */
  for (int i = 0; i < (int)size; i += 2) {
    for (int j = 0; j < 2; j++)
      buf.write[j] = in[i + j];
    buf.write = bufWrap(buf.write + 2);
  }

  /* UI logic */
  hw.ProcessAllControls();
  int new_dir = hw.encoder.Increment();
  if (new_dir)
    buf.dir = new_dir;
  if (hw.button1.RisingEdge() &&
      buf.dir == -1) { /* start of reverse playback */
    /* buf.write - 2 is the newest frame in the buffer. */
    buf.read = bufWrap(buf.write - 2);
    buf.read_frac = 0;
  } else if (hw.button1.FallingEdge()) { /* return to normal forward playback */
    buf.read = old_write;
    buf.read_frac = 0;
  }

  if (hw.button1.Pressed())
    speed = hw.knob1.Process();

  uint32_t now = System::GetNow();
  if (now - buf.last_knob2_update >= 10) {
    buf.last_knob2_update = now;
    float knob2 = hw.knob2.Process();
    if (hw.button2.Pressed())
      speed = 1000.0 * (knob2 - buf.last_knob2);
    buf.last_knob2 = knob2;
  }

  /* Write to output buffer */
  if (hw.button1.Pressed() || hw.button2.Pressed()) {
    for (int i = 0; i < (int)size; i += 2) {
      for (buf.read_frac += speed; buf.read_frac > 1.0; buf.read_frac -= 1.0)
        buf.read = bufWrap(buf.read + buf.dir * 2);
      for (int j = 0; j < 2; j++)
        out[i + j] = buf.read_frac * buf.read[j] +
                     (1.0 - buf.read_frac) * bufWrap(buf.read - buf.dir * 2)[j];
    }
  } else {
    for (int i = 0; i < (int)size; i += 2) {
      for (int j = 0; j < 2; j++)
        out[i + j] = buf.read[j];
      buf.read = bufWrap(buf.read + 2);
    }
  }
}

int main(void) {
  buf_init();
  hw.Init();
  hw.StartAdc();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
