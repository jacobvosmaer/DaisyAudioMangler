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
#define endof(x) (x + nelem(x))

float DSY_SDRAM_BSS
    buffer[(1 << 26) / sizeof(float)]; /* Use all 64MB of sample RAM */

float scrubrange = 0.25;
float speed = 1.0;
float scrub = 0.5;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  buf_setscrubrange(scrubrange);
  buf_callback(in, out, size, speed, scrub);
}

uint8_t held_keys[128 / 8];

void note_on(int key) {
  held_keys[key / nelem(held_keys)] |= 1 << (key % 8);
  switch (key % 3) {
  case 0:
    buf_setdirection(1);
    buf_setmode(BUF_VARISPEED, scrub);
    break;
  case 1:
    buf_setdirection(-1);
    buf_setmode(BUF_VARISPEED, scrub);
    break;
  case 2:
    buf_setmode(BUF_SCRUB, scrub);
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

void control_change(int cc, int val) {
  if (cc == 1)
    speed = 1.0 - (float)val / 127.0;
}

float bendhist[16];
unsigned bendhistpos;

void pitch_bend(int val) {
  enum { max = (1 << 14) - 1 };
  float *f;
  assert(val >= 0 && val <= max);
  assert(max > 0);
  bendhist[bendhistpos++ % nelem(bendhist)] = (float)val / (float)max;
  scrub = 0;
  for (f = bendhist; f < endof(bendhist); f++)
    scrub += *f;
  scrub /= (float)nelem(bendhist);
}

midi_parser mp;
void midicallback(uint8_t *uartdata, size_t size, void *context) {
  while (size--) {
    midi_message msg = midi_read(&mp, *uartdata++);
    /* TODO listen on specific MIDI channel? */
    int status = msg.status & 0xf0;
    if (status == MIDI_NOTE_ON && msg.data[1])
      note_on(msg.data[0]);
    else if (status == MIDI_NOTE_OFF || status == MIDI_NOTE_ON)
      note_off(msg.data[0]);
    else if (status == MIDI_CONTROL_CHANGE)
      control_change(msg.data[0], msg.data[1]);
    else if (status == MIDI_PITCH_BEND)
      pitch_bend((msg.data[0] << 7) | msg.data[1]);
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
