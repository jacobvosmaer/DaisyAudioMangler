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

float DSY_SDRAM_BSS buffer[2 * (1 + 10 * 48000)]; /* 10s at 48kHz */

struct {
  float *start, *end, *write, *read, read_frac;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer, 0};

void copyRing(void *dstStart, void *dstEnd, void **dstPos, const void *srcStart,
              const void *srcEnd, const void **srcPos, size_t nElem,
              size_t elemSize) {
  struct {
    char *start, *end, **pos;
  } dst = {(char *)dstStart, (char *)dstEnd,
           (char **)(dstPos ? dstPos : &dstStart)},
    src = {(char *)srcStart, (char *)srcEnd,
           (char **)(srcPos ? srcPos : &srcStart)};
  ptrdiff_t nBytes;

  if (!nElem || !elemSize)
    return;

  assert(nElem < PTRDIFF_MAX / elemSize);
  nBytes = nElem * elemSize;

  assert(src.start <= *src.pos && *src.pos <= src.end);
  assert(dst.start <= *dst.pos && *dst.pos <= dst.end);
  while (nBytes) {
    ptrdiff_t chunk = min(nBytes, min(src.end - *src.pos, dst.end - *dst.pos));
    memmove(*dst.pos, *src.pos, chunk);
    *dst.pos = *dst.pos + chunk == dst.end ? dst.start : *dst.pos + chunk;
    *src.pos = *src.pos + chunk == src.end ? src.start : *src.pos + chunk;
    nBytes -= chunk;
  }
}

float *bufWrap(float *p) {
  ptrdiff_t buf_size = buf.end - buf.start;
  if (p < buf.start)
    p += buf_size;
  else if (p >= buf.end)
    p -= buf_size;
  return p;
}

float combine(float ratio, float x0, float x1) {
  return ratio * x0 + (1.0 - ratio) * x1;
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size) {
  float *old_write = buf.write;

  copyRing(buf.start, buf.end, (void **)&buf.write, in, in + size, 0, size,
           sizeof(*in));

  hw.ProcessAllControls();
  float speed = hw.knob1.Process();
  if (hw.button1.RisingEdge()) { /* start of reverse playback */
    /* buf.write - 2 is the newest frame in the buffer. */
    buf.read = bufWrap(buf.write - 2);
  } else if (hw.button1.FallingEdge()) { /* return to forward playback */
    buf.read = old_write;
  }

  if (hw.button1.Pressed()) { /* reverse mode active */
    for (size_t i = 0; i < size; i += 2) {
      for (buf.read_frac += speed; buf.read_frac > 1.0; buf.read_frac -= 1.0)
        buf.read = bufWrap(buf.read - 2);
      for (int j = 0; j < 2; j++)
        out[i + j] =
            combine(buf.read_frac, buf.read[j], bufWrap(buf.read + 2)[j]);
    }
  } else { /* normal forward playback */
    copyRing(out, out + size, 0, buf.start, buf.end, (const void **)&buf.read,
             size, sizeof(*out));
  }
}

int main(void) {
  hw.Init();
  hw.StartAdc();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
