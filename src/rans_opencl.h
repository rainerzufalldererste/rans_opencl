#ifndef rans_opencl_h__
#define rans_opencl_h__

#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>

bool rans_opencl_init(const size_t inputDataSize, const size_t outputDataSize);
bool rans_opencl_uncompress_O0_32x16(unsigned char *in, unsigned int in_size, unsigned int out_size);
bool rans_opencl_download_buffer(uint8_t *pBuffer, const size_t size);
void rans_opencl_destroy();

#endif // rans_opencl_h__