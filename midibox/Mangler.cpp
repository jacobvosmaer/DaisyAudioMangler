#include "audio.h"
#include "daisy_seed.h"
extern "C" {
#include "midi.h"
}
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisySeed hw;
MidiUartTransport midi;

#define nelem(x) (sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

float scrubrange = 1.0;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  /*
    int encoder = hw.encoder.Increment();
    if (encoder) {
      if (hw.button1.Pressed())
        buf_setdirection(encoder);
      else if (hw.button2.Pressed())
        scrubrange *= encoder > 0 ? 1.414 : 0.7071;
      buf_setscrubrange(scrubrange);
    }
  */

  /*
    if (hw.button1.RisingEdge())
      buf_setmode(BUF_VARISPEED, 0);
    else if (hw.button2.RisingEdge())
      buf_setmode(BUF_SCRUB, hw.knob2.Process());
    else if (hw.button1.FallingEdge() || hw.button2.FallingEdge())
      buf_setmode(BUF_PASSTHROUGH, 0);
  */

  buf_callback(in, out, size, 1, 0);
}

midi_parser mp;
unsigned received;
void midicallback(uint8_t *data, size_t size, void *context) {
  while (size--) {
    midi_message msg = midi_read(&mp, *data++);
    if ((msg.status & 0xf0) == MIDI_NOTE_ON)
      received++;
  }
}

int main(void) {
  hw.Init();
  buf_init(buffer, nelem(buffer), hw.AudioSampleRate());
  midi.Init(MidiUartTransport::Config());
  midi.StartRx(midicallback, 0);
  hw.StartAudio(AudioCallback);

  while (1)
    hw.SetLed(received % 2);
}
