CFLAGS += -std=c99 -Wall -Werror -pedantic

.PHONY: all
all: audio.o
	$(MAKE) -C libDaisy
	$(MAKE) -C pod
	$(MAKE) -C midibox
