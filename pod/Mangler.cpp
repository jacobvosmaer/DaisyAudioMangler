#include "../audio.h"
#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  hw.ProcessAllControls();

  int encoder = hw.encoder.Increment();
  if (encoder) {
    if (hw.button1.Pressed())
      buf.dir = encoder;
    else if (hw.button2.Pressed())
      buf.scrub_range_s *= encoder > 0 ? 1.414 : 0.7071;
  }
  if (hw.button1.FallingEdge() || hw.button2.FallingEdge()) {
    /* We are returning to normal playback but buf.read is possibly
     * desynchronized from buf.write. */
    buf.read = buf.write;
    buf.read_frac = 0;
  }

  if (hw.button2.RisingEdge()) { /* start scrubbing mode */
    /* We want to scrub relative to the current position of knob2. */
    buf.scrub_origin = hw.knob2.Process();
    buf.scrub = 0;
  }

  if (hw.button1.Pressed())
    buf.mode = BUF_VARISPEED;
  else if (hw.button2.Pressed())
    buf.mode = BUF_SCRUB;
  else
    buf.mode = BUF_PASSTHROUGH;

  buf.knob1 = hw.knob1.Process();
  buf.knob2 = hw.knob2.Process();
  buf_callback(in, out, size);
}

int main(void) {
  hw.Init();
  buf_init(buffer, nelem(buffer), hw.AudioSampleRate());
  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
