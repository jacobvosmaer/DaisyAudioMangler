#include "../audio.h"
#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

float scrubrange = 1.0;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  hw.ProcessAllControls();

  int encoder = hw.encoder.Increment();
  if (encoder) {
    if (hw.button1.Pressed())
      buf_setdirection(encoder);
    else if (hw.button2.Pressed())
      scrubrange *= encoder > 0 ? 1.414 : 0.7071;
    buf_setscrubrange(scrubrange);
  }

  if (hw.button1.RisingEdge())
    buf_setmode(BUF_VARISPEED, 0);
  else if (hw.button2.RisingEdge())
    buf_setmode(BUF_SCRUB, hw.knob2.Process());
  else if (hw.button1.FallingEdge() || hw.button2.FallingEdge())
    buf_setmode(BUF_PASSTHROUGH, 0);

  buf_callback(in, out, size, hw.knob1.Process(), hw.knob2.Process());
}

int main(void) {
  hw.Init();
  buf_init(buffer, nelem(buffer), hw.AudioSampleRate());
  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
