#include "daisy_pod.h"
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

typedef float frame[2];

frame DSY_SDRAM_BSS buffer[10 * 48000]; /* 30s at 48kHz */

struct {
  frame *start, *end, *write, *read;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer};

/* Daisy audio is 2 channels in, 2 channels out. An audio frame is 2 floats (one
 * for each channel). The in and out buffers are float*. Size is the number of
 * incoming frames, so in points to size*2 floats. */
static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  size_t n, remaining;
  frame *old_write = buf.write;

  hw.ProcessDigitalControls();

  n = size;
  remaining = (buf.end - buf.write);
  if (n > remaining) {
    memcpy(buf.write, in, remaining * sizeof(frame));
    n -= remaining;
    in += remaining;
    buf.write = buf.start;
  }
  memcpy(buf.write, in, n * sizeof(frame));
  buf.write += n;

  if (hw.button1.RisingEdge() || hw.button1.FallingEdge())
    buf.read = old_write;

  if (hw.button1.Pressed()) {
    for (n = size; n--; out += 2) {
      if (--buf.read < buf.start)
        buf.read = buf.end - 1;
      out[0] = *buf.read[0];
      out[1] = *buf.read[1];
    }
  } else {
    n = size;
    remaining = (buf.end - buf.read);
    if (n > remaining) {
      memcpy(out, buf.read, remaining * sizeof(frame));
      n -= remaining;
      out += remaining;
      buf.read = buf.start;
    }
    memcpy(out, buf.read, n * sizeof(frame));
    buf.read += n;
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
