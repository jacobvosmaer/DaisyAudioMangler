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

float speed = 1.0;
int direction = 1;
int mode = 0;
enum { midichannel = 16 }; /* Hard-code MIDI channel, 1-based */

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  buf_setmode(mode);
  buf_setdirection(direction);
  buf_setspeed(speed);
  buf_callback(in, out, size);
}

#define NUMNOTES 128
struct {
  uint8_t q[NUMNOTES];
  int len;
} notes;

void delnote(uint8_t n) {
  assert(notes.len >= 0 && notes.len <= nelem(notes.q));
  uint8_t *p = notes.q;
  while (p < notes.q + notes.len)
    if (*p == n)
      memmove(p, p + 1, notes.q + notes.len-- - (p + 1));
    else
      p++;
}

void pushnote(uint8_t n) {
  assert(notes.len >= 0 && notes.len < nelem(notes.q));
  notes.q[notes.len++] = n;
}

uint8_t popnote(void) {
  assert(notes.len > 0 && notes.len <= nelem(notes.q));
  return notes.q[--notes.len];
}

void note_on(uint8_t key) {
  struct {
    int mode, direction;
  } * p, paramtab[] = {{BUF_VARISPEED, 1},
                       {BUF_VARISPEED, -1},
                       {BUF_MUTE, 0},
                       {BUF_STOP, 0}};
  assert(key < NUMNOTES);
  delnote(key);
  pushnote(key);
  p = &paramtab[key % nelem(paramtab)];
  mode = p->mode;
  if (mode == BUF_VARISPEED)
    direction = p->direction;
}

void note_off(uint8_t key) {
  assert(key < NUMNOTES);
  delnote(key);
  if (notes.len)
    note_on(popnote());
  else
    mode = BUF_PASSTHROUGH;
}

void control_change(int cc, int val) {
  if (cc == 1) /* mod wheel */
    speed = 1.0 - (float)val / 127.0;
  else if (cc == 123) { /* all notes off */
    notes.len = 0;
    note_off(0);
  }
}

midi_parser mp;
void midicallback(uint8_t *uartdata, size_t size, void *context) {
  while (size--) {
    midi_message msg = midi_read(&mp, *uartdata++);
    if ((msg.status & (midichannel - 1)) != (midichannel - 1))
      continue;
    switch (msg.status & 0xf0) {
    case MIDI_NOTE_ON:
      if (msg.data[1])
        note_on(msg.data[0]);
      else
        note_off(msg.data[0]);
      break;
    case MIDI_NOTE_OFF:
      note_off(msg.data[0]);
      break;
    case MIDI_CONTROL_CHANGE:
      control_change(msg.data[0], msg.data[1]);
      break;
    }
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
  buf_init(buffer, nelem(buffer), hw.AudioSampleRate());
  hw.StartAudio(AudioCallback);
  midi.Init(MidiUartTransport::Config());
  midi.StartRx(midicallback, 0);

  while (1)
    ;
}
