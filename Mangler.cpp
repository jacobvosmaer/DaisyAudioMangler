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
  float *start, *end, *write, *read;
  float speed, read_frac;
} buf = {buffer, buffer + nelem(buffer), buffer, buffer, 0.8, 0};

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

void buf_decr_read(void) {
  buf.read -= 2;
  if (buf.read < buf.start)
    buf.read = buf.end - 2;
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

  hw.ProcessDigitalControls();
  if (hw.button1.RisingEdge()) { /* start of reverse playback */
    buf.read = buf.write;
    buf_decr_read();
  } else if (hw.button1.FallingEdge()) { /* return to forward playback */
    buf.read = old_write;
  }

  if (hw.button1.Pressed()) { /* reverse mode active */
    for (size_t i = 0; i < size; i += 2) {
      /* This does not look correct to me. I think the while loop needs to
       * update prev_sample? */
      float prev_sample[2] = {buf.read[0], buf.read[1]};
      buf.read_frac += buf.speed;
      while (buf.read_frac > 1.0) {
        buf_decr_read();
        buf.read_frac -= 1.0;
      }
      out[i] = combine(buf.read_frac, buf.read[0], prev_sample[0]);
      out[i + 1] = combine(buf.read_frac, buf.read[1], prev_sample[1]);
    }
  } else { /* normal forward playback */
    copyRing(out, out + size, 0, buf.start, buf.end, (const void **)&buf.read,
             size, sizeof(*out));
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(4);
  hw.StartAudio(AudioCallback);

  while (1)
    ;
}
