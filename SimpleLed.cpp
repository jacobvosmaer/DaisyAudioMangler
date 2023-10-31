#include "daisy_pod.h"
#include <string.h>

using namespace daisy;

DaisyPod hw;
Parameter p_knob1, p_knob2;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
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
