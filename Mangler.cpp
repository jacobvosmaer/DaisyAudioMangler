#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS buffer[2 * (1 + 10 * 48000)]; /* 10s at 48kHz */

struct {
  float *start, *end, *write, *read, read_frac;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer, 0};

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

  for (int i = 0; i < (int)size; i += 2) {
    for (int j = 0; j < 2; j++)
      buf.write[j] = in[i + j];
    buf.write = bufWrap(buf.write + 2);
  }

  hw.ProcessAllControls();
  float speed = hw.knob1.Process();
  if (hw.button1.RisingEdge()) { /* start of reverse playback */
    /* buf.write - 2 is the newest frame in the buffer. */
    buf.read = bufWrap(buf.write - 2);
    buf.read_frac = 0;
  } else if (hw.button1.FallingEdge() ||
             hw.button2.FallingEdge()) { /* return to forward playback */
    buf.read = old_write;
    buf.read_frac = 0;
  }

  if (hw.button1.Pressed() || hw.button2.Pressed()) {
    int dir = hw.button1.Pressed() ? -1 : 1;
    for (int i = 0; i < (int)size; i += 2) {
      for (buf.read_frac += speed; buf.read_frac > 1.0; buf.read_frac -= 1.0)
        buf.read = bufWrap(buf.read + dir * 2);
      for (int j = 0; j < 2; j++)
        out[i + j] = buf.read_frac * buf.read[j] +
                     (1.0 - buf.read_frac) * bufWrap(buf.read - dir * 2)[j];
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
  hw.StartAdc();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
