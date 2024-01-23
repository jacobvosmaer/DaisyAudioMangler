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

#define nelem(x) (ptrdiff_t)(sizeof(x) / sizeof(*x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

float scrubrange = 1.0;
int mute;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {

  buf_callback(in, out, size, 1, 0);
}

uint8_t held_keys[128 / 8];

void note_on(int key) {
  held_keys[key / nelem(held_keys)] |= 1 << (key % 8);
  switch (key % 3) {
  case 0:
    buf_setdirection(1);
    buf_setmode(BUF_VARISPEED, 0);
    break;
  case 1:
    buf_setdirection(-1);
    buf_setmode(BUF_VARISPEED, 0);
    break;
  case 2:
    buf_setmode(BUF_MUTE, 0);
    break;
  }
}

void note_off(int key) {
  int i, any_held = 0;
  held_keys[key / nelem(held_keys)] &= ~(1 << (key % 8));

  for (i = 0; i < nelem(held_keys); i++)
    any_held |= held_keys[i];

  if (!any_held)
    buf_setmode(BUF_PASSTHROUGH, 0);
}

midi_parser mp;
void midicallback(uint8_t *data, size_t size, void *context) {
  while (size--) {
    midi_message msg = midi_read(&mp, *data++);
    int status = msg.status & 0xf0;
    if (status == MIDI_NOTE_ON && msg.data[1])
      note_on(msg.data[0]);
    else if (status == MIDI_NOTE_OFF || status == MIDI_NOTE_ON)
      note_off(msg.data[0]);
  }
}

int main(void) {
  hw.Init();
  buf_init(buffer, nelem(buffer), hw.AudioSampleRate());
  midi.Init(MidiUartTransport::Config());
  midi.StartRx(midicallback, 0);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
