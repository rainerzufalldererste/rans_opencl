#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "rans_static_32x16.h"
#include "rans_opencl.h"

#define TEST_COMPRESSION_SIZE (1024 * 1024 * 4 + 7)

uint8_t *uncompressedData = NULL;
uint8_t *decompressedData = NULL;
uint8_t *compressedData = NULL;

int main(int argc, char **pArgv)
{
  (void)argc;
  (void)pArgv;

  uncompressedData = malloc(TEST_COMPRESSION_SIZE);
  decompressedData = malloc(TEST_COMPRESSION_SIZE);
  compressedData = malloc(rans_compress_bound_32x16_AVX2(TEST_COMPRESSION_SIZE, 0, NULL)); // The AVX2 version happens to return a slightly larger size.

  if (!uncompressedData || !decompressedData || !compressedData)
    goto epilogue;

  const char testString[] = "THIS IS A TEST STRING TEST STRING TEST STRING";

  for (size_t i = 0; i < TEST_COMPRESSION_SIZE; i++)
    uncompressedData[i] = (uint8_t)(testString[i % sizeof(testString)]);

  uint32_t size = rans_compress_bound_32x16_AVX2(TEST_COMPRESSION_SIZE, 0, NULL);
  rans_compress_to_32x16_NoSimd(uncompressedData, TEST_COMPRESSION_SIZE, compressedData, &size, 0);

  printf("COMPRESSED TO %f.\n", size / (float)TEST_COMPRESSION_SIZE);

  uint32_t outSize = TEST_COMPRESSION_SIZE;
  rans_uncompress_to_32x16_NoSimd(compressedData, size, decompressedData, &outSize, 0);

  if (memcmp(uncompressedData, decompressedData, TEST_COMPRESSION_SIZE) != 0)
    puts("CPU: INVALID.");
  else
    puts("CPU: VALID.");

  memset(decompressedData, 0, TEST_COMPRESSION_SIZE);

  if (!rans_opencl_init(size, TEST_COMPRESSION_SIZE))
  {
    puts("FAILED IN `rans_opencl_init`.");
    goto epilogue;
  }

  if (!rans_opencl_uncompress_O0_32x16(compressedData, size, TEST_COMPRESSION_SIZE))
  {
    puts("FAILED IN `rans_opencl_uncompress_O0_32x16`.");
    goto epilogue;
  }

  if (!rans_opencl_download_buffer(decompressedData, TEST_COMPRESSION_SIZE))
  {
    puts("FAILED IN `rans_opencl_download_buffer`.");
    goto epilogue;
  }

  if (memcmp(uncompressedData, decompressedData, TEST_COMPRESSION_SIZE) != 0)
    puts("GPU: INVALID.");
  else
    puts("GPU: VALID.");

epilogue:
  rans_opencl_destroy();
  free(uncompressedData);
  free(decompressedData);
  free(compressedData);

  return 0;
}