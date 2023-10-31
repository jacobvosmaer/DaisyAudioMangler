#include "daisy_pod.h"
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS buffer[30 * 48000 * 2]; /* 30s at 48kHz */

struct {
  float *start, *end, *write, *read;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer};

/* Daisy audio is 2 channels in, 2 channels out. An audio frame is 2 floats (one
 * for each channel). The in and out buffers are float*. Size is the number of
 * incoming frames, so in points to size*2 floats. */
static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  size_t n, remaining;
  float *old_write = buf.write;

  hw.ProcessDigitalControls();

  n = size;
  remaining = (buf.end - buf.write) / 2;
  if (n > remaining) {
    memcpy(buf.write, in, 2 * remaining * sizeof(*in));
    n -= remaining;
    in += 2 * remaining;
    buf.write = buf.start;
  }
  memcpy(buf.write, in, 2 * n * sizeof(*in));
  buf.write += 2 * n;

  if (hw.button1.RisingEdge() || hw.button1.FallingEdge())
    buf.read = old_write;

  if (hw.button1.Pressed()) {
    memset(out, 0, 2 * size * sizeof(*out));
  } else {
    n = size;
    remaining = (buf.end - buf.read) / 2;
    if (n > remaining) {
      memcpy(out, buf.read, 2 * remaining * sizeof(*out));
      n -= remaining;
      out += 2 * remaining;
      buf.read = buf.start;
    }
    memcpy(out, buf.read, 2 * n * sizeof(*out));
    buf.read += 2 * n;
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
