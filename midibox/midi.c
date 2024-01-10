#include "midi.h"

midi_message midi_read(midi_parser *p, uint8_t b) {
	enum { DATA0_PRESENT = 0x80 };
	midi_message msg = {0};

	if (b >= 0xf8) {
		msg.status = b;
	} else if (b >= 0xf4) {
		msg.status = b;
		p->status = 0;
	} else if (b >= 0x80) {
		p->status = b;
		p->previous = b == 0xf0 ? b : 0;
	} else if ((p->status >= 0xc0 && p->status < 0xe0) ||
		   (p->status >= 0xf0 && p->status != 0xf2)) {
		msg.status = p->status;
		msg.data[0] = b;
		msg.data[1] = p->previous;
		p->previous = 0;
	} else if (p->status && p->previous & DATA0_PRESENT) {
		msg.status = p->status;
		msg.data[0] = p->previous ^ DATA0_PRESENT;
		msg.data[1] = b;
		p->previous = 0;
	} else {
		p->previous = b | DATA0_PRESENT;
	}

	return msg;
}
