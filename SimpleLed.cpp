#include "daisy_pod.h"
#include <string.h>

using namespace daisy;

DaisyPod hw;
Parameter p_knob1, p_knob2;

#define nelem(x) (sizeof(x) / sizeof(x[0]))

float DSY_SDRAM_BSS buf[(64 * 1024 * 1024) / sizeof(float)];
float *buf_head = buf, *buf_tail = buf, *buf_end = buf + nelem(buf);

/* Daisy audio is 2 channels in, 2 channels out. An audio frame is 2 floats (one
 * for each channel). The in and out buffers are float*. Size is the number of
 * incoming frames, so in points to size*2 floats. */
static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  size_t to_buffer = size;
  size_t remaining = buf_end - buf_head;
  if (to_buffer > remaining / 2) {
    memcpy(buf_head, in, remaining * sizeof(*in));
    to_buffer -= remaining / 2;
    in += remaining;
    buf_head = buf;
  }

  memcpy(buf_head, in, to_buffer * sizeof(*in) * 2);

  memcpy(out, in, size * sizeof(*in) * 2);
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
