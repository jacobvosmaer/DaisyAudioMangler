#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

struct {
  float *start, *end, *write, *read, read_frac;
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

  /* Write to output buffer */
  if (hw.button1.Pressed()) {
    float speed = hw.knob1.Process();
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
  hw.Init();
  buf_init();
  hw.StartAdc();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
