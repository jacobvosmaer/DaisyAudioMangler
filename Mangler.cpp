#include "daisy_pod.h"
#include <limits.h>
#include <string.h>

using namespace daisy;

DaisyPod hw;

#define min(x, y) ((x) < (y) ? (x) : (y))
#define nelem(x) (sizeof(x) / sizeof(*x))
#undef assert
#define assert(x)                                                              \
  if (!(x))                                                                    \
  __builtin_trap()

typedef struct {
  float s0, s1;
} frame;

frame DSY_SDRAM_BSS buffer[1 + 10 * 48000]; /* 10s at 48kHz */

struct {
  frame *start, *end, *write, *read;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer};

void copyRing(void *dstStart, void *dstEnd, void **dstPos, const void *srcStart,
              const void *srcEnd, const void **srcPos, size_t nElem,
              size_t elemSize) {
  struct {
    char *start, *end;
  } dst = {(char *)dstStart, (char *)dstEnd},
    src = {(char *)srcStart, (char *)srcEnd};
  char **dPos = dstPos ? (char **)dstPos : (char **)&dstStart,
       **sPos = srcPos ? (char **)srcPos : (char **)&srcStart;
  ptrdiff_t nBytes;

  if (!nElem || !elemSize)
    return;

  assert(nElem < PTRDIFF_MAX / elemSize);
  nBytes = nElem * elemSize;

  assert(src.start <= *sPos && *sPos <= src.end);
  assert(dst.start <= *dPos && *dPos <= dst.end);
  while (nBytes) {
    ptrdiff_t chunk = min(nBytes, min(src.end - *sPos, dst.end - *dPos));
    memmove(*dPos, *sPos, chunk);
    *dPos = *dPos + chunk == dst.end ? dst.start : *dPos + chunk;
    *sPos = *sPos + chunk == src.end ? src.start : *sPos + chunk;
    nBytes -= chunk;
  }
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  frame *old_write = buf.write;

  copyRing(buf.start, buf.end, (void **)&buf.write, in, in + size, 0, size,
           sizeof(frame));

  hw.ProcessDigitalControls();
  if (hw.button1.FallingEdge())
    buf.read = old_write;

  if (hw.button1.Pressed()) {
    for (; size--; out += 2) {
      if (--buf.read < buf.start)
        buf.read = buf.end - 1;
      out[0] = buf.read->s0;
      out[1] = buf.read->s1;
    }
  } else {
    copyRing(out, out + size, 0, buf.start, buf.end, (const void **)&buf.read,
             size, sizeof(frame));
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(16);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
