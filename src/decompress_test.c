#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define NX 32

#define TF_SHIFT 12
#define TOTFREQ (1<<TF_SHIFT)

#define RANS_BYTE_L (1u << 15)
typedef uint32_t RansState;

static inline void RansDecInit_single(RansState* r, uint8_t** pptr)
{
  uint32_t x;
  uint8_t* ptr = *pptr;

  x = ptr[0] << 0;
  x |= ptr[1] << 8;
  x |= ptr[2] << 16;
  x |= ptr[3] << 24;
  ptr += 4;

  *pptr = ptr;
  *r = x;
}

unsigned char *rans_uncompress_O0_32x16_NoSimd_single(unsigned char *in, unsigned int in_size,
  unsigned char *out, unsigned int *out_size) {
  (void)in_size;

  /* Load in the static tables */
  unsigned char *cp = in + 4;
  int i, j, x, y, out_sz, rle;
  //uint16_t sfreq[TOTFREQ+32];
  //uint16_t sbase[TOTFREQ+32]; // faster to use 32-bit on clang
  uint8_t  ssym[TOTFREQ + 64]; // faster to use 16-bit on clang

  uint32_t s3[TOTFREQ]; // For TF_SHIFT <= 12

  out_sz = ((in[0]) << 0) | ((in[1]) << 8) | ((in[2]) << 16) | ((in[3]) << 24);
  if (!out) {
    out = malloc(out_sz);
    *out_size = out_sz;
  }
  if (!out || (int64_t)out_sz > (int64_t)*out_size)
    return NULL;

  // Precompute reverse lookup of frequency.
  rle = x = y = 0;
  j = *cp++;
  do {
    int F, C;
    if ((F = *cp++) >= 128) {
      F &= ~128;
      F = ((F & 127) << 8) | *cp++;
    }
    C = x;

    for (y = 0; y < F; y++) {
      ssym[y + C] = (uint8_t)j;
      //sfreq[y + C] = F;
      //sbase[y + C] = y;
      s3[y + C] = (((uint32_t)F) << (TF_SHIFT + 8)) | (y << 8) | j;
    }
    x += F;

    if (!rle && j + 1 == *cp) {
      j = *cp++;
      rle = *cp++;
    }
    else if (rle) {
      rle--;
      j++;
    }
    else {
      j = *cp++;
    }
  } while (j);

  assert(x < TOTFREQ);

  int z;

  RansState R[NX];
  for (z = 0; z < NX; z++)
    RansDecInit_single(&R[z], &cp);

  int out_end = (out_sz&~(NX - 1));
  const uint32_t mask = (1u << TF_SHIFT) - 1;

  uint16_t *cp16 = (uint16_t *)cp;

  for (int i = 0; i < out_end; i += NX)
  {
    for (int index = 0; index < NX; index++)
    {
      uint32_t S = s3[R[index] & mask];

      R[index] = (S >> (TF_SHIFT + 8)) * (R[index] >> TF_SHIFT) + ((S >> 8) & mask);

      out[i + index] = (unsigned char)S;
    }

    for (int j = 0; j < NX; j++)
      if (R[j] < RANS_BYTE_L)
        R[j] = (R[j] << 16) | *cp16++;
  }

  // assume NX is divisible by 4
  assert(NX % 4 == 0);

  for (z = out_sz & (NX - 1); z-- > 0; )
    out[out_end + z] = ssym[R[z] & mask];

  *out_size = out_sz;
  return out;
}
