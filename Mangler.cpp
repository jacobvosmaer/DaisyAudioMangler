#include "daisy_pod.h"
#include <string.h>

using namespace daisy;

DaisyPod hw;
Parameter p_knob1, p_knob2;

#define BUFFER_SIZE (30 * 48000 * 2) /* 30s at 48kHz */
float DSY_SDRAM_BSS buffer[BUFFER_SIZE];

struct {
  float *buffer, *head, *tail, *end;
} buf = {buffer, buffer, buffer, buffer + BUFFER_SIZE};

/* Daisy audio is 2 channels in, 2 channels out. An audio frame is 2 floats (one
 * for each channel). The in and out buffers are float*. Size is the number of
 * incoming frames, so in points to size*2 floats. */
static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  size_t to_buffer = size, from_buffer = size;
  size_t remaining;

  if (remaining = buf.end - buf.head, to_buffer > remaining / 2) {
    memcpy(buf.head, in, remaining * sizeof(*in));
    to_buffer -= remaining / 2;
    in += remaining;
    buf.head = buf.buffer;
  }
  memcpy(buf.head, in, 2 * to_buffer * sizeof(*in));
  buf.head += 2 * to_buffer;

  if (remaining = buf.end - buf.tail, from_buffer > remaining / 2) {
    memcpy(out, buf.tail, remaining * sizeof(*out));
    from_buffer -= remaining / 2;
    out += remaining;
    buf.tail = buf.buffer;
  }
  memcpy(out, buf.tail, 2 * from_buffer * sizeof(*out));
  buf.tail += 2 * from_buffer;
}

int main(void) {
  hw.Init();
  float r = 0, g = 0, b = 0;
  p_knob1.Init(hw.knob1, 0, 1, Parameter::LINEAR);
  p_knob2.Init(hw.knob2, 0, 1, Parameter::LINEAR);
  hw.SetAudioBlockSize(4);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (1) {
    r = p_knob1.Process();
    g = p_knob2.Process();

    hw.led1.Set(r, g, b);

    hw.UpdateLeds();
  }
}
