#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>

enum {
	MIDI_NOTE_OFF = 0x80,
	MIDI_NOTE_ON = 0x90,
	MIDI_AFTERTOUCH = 0xa0,
	MIDI_CONTROL_CHANGE = 0xb0,
	MIDI_PROGRAM_CHANGE = 0xc0,
	MIDI_CHANNEL_PRESSURE = 0xd0,
	MIDI_PITCH_BEND = 0xe0,
	MIDI_SYSEX = 0xf0,
	MIDI_SYSEX_END = 0xf7
};

typedef struct midi_message {
	uint8_t status;
	uint8_t data[2];
} midi_message;

typedef struct midi_parser {
	uint8_t status;
	uint8_t previous;
} midi_parser;

midi_message midi_read(midi_parser *p, uint8_t b);

#endif
