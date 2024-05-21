# DaisyAudioMangler

This project implements several audio effects in one on the Daisy platform.

- Variable speed playback
- Reverse playback
- Tape stop
- Live sampling

The audio is continuously recorded from the Daisy audio input.

For more information see these blog posts: [1](https://blog.jacobvosmaer.nl/0012-recurse-projects/#mangler), [2](https://blog.jacobvosmaer.nl/0021-recurse-projects-part-3/#mangler).

The effects are meant to be controlled via midi although some of them can be triggered from the Daisy Pod buttons and knobs too.

The `pod/` directory contains a Daisy Pod firmware that runs the audio engine. For MIDI control use `midibox/` instead.
