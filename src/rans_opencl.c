#include "CL/cl.h"

#include "rans_opencl.h"
#include "rans_opencl_kernel.h"

#define NX 32

#define TF_SHIFT 12
#define TOTFREQ (1 << TF_SHIFT)

#define RANS_BYTE_L (1u << 15)
typedef uint32_t RansState;

bool initialized = false;

cl_context context = NULL;
cl_platform_id *pPlatforms = NULL;
cl_device_id deviceID = NULL;
cl_command_queue commandQueue = NULL;
cl_mem gpu_in_buffer = NULL;
cl_mem gpu_in_buffer_R = NULL;
cl_mem gpu_in_buffer_S3 = NULL;
cl_mem gpu_in_buffer_SSYM = NULL;
cl_mem gpu_out_buffer = NULL;
cl_program program = NULL;
cl_kernel decodeKernel = NULL;

bool rans_opencl_init(const size_t inputDataSize, const size_t outputDataSize)
{
  bool success = true;
  cl_int result = CL_SUCCESS;
  cl_uint platformCount;
  const size_t kernelSourceLength = strlen(kernelsource) + 1;

  if (CL_SUCCESS != (result = (result = clGetPlatformIDs(0, NULL, &platformCount)) || platformCount == 0))
    goto epilogue;

  pPlatforms = malloc(sizeof(cl_platform_id) * platformCount);

  if (pPlatforms == NULL)
    goto epilogue;

  result = clGetPlatformIDs(platformCount, pPlatforms, NULL);
  result |= clGetDeviceIDs(pPlatforms[0], CL_DEVICE_TYPE_GPU, 1, &deviceID, &platformCount);

  if (result != CL_SUCCESS)
    goto epilogue;

  context = clCreateContext(NULL, 1, &deviceID, NULL, NULL, &result);

  if (result != CL_SUCCESS || context == NULL)
    goto epilogue;

  commandQueue = clCreateCommandQueue(context, deviceID, 0, &result);

  if (result != CL_SUCCESS || commandQueue == NULL)
    goto epilogue;

  gpu_in_buffer = clCreateBuffer(context, CL_MEM_READ_ONLY, inputDataSize, NULL, &result);

  if (result != CL_SUCCESS || gpu_in_buffer == NULL)
    goto epilogue;

  gpu_in_buffer_R = clCreateBuffer(context, CL_MEM_READ_ONLY, NX * sizeof(uint32_t), NULL, &result);

  if (result != CL_SUCCESS || gpu_in_buffer == NULL)
    goto epilogue;

  gpu_in_buffer_S3 = clCreateBuffer(context, CL_MEM_READ_ONLY, TOTFREQ * sizeof(uint32_t), NULL, &result);

  if (result != CL_SUCCESS || gpu_in_buffer == NULL)
    goto epilogue;

  gpu_in_buffer_SSYM = clCreateBuffer(context, CL_MEM_READ_ONLY, TOTFREQ + 64, NULL, &result);

  if (result != CL_SUCCESS || gpu_in_buffer == NULL)
    goto epilogue;

  gpu_out_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, outputDataSize, NULL, &result);

  if (result != CL_SUCCESS || gpu_out_buffer == NULL)
    goto epilogue;

  program = clCreateProgramWithSource(context, 1, (const char **)&kernelsource, (const size_t *)&kernelSourceLength, &result);

  if (result != CL_SUCCESS || program == NULL)
    goto epilogue;

  result = clBuildProgram(program, 1, &deviceID, NULL, NULL, NULL);

  if (result == CL_BUILD_PROGRAM_FAILURE)
  {
#ifdef _DEBUG
    // Determine the size of the log
    size_t logSize;
    result = clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);

    // Allocate memory for the log
    char *log = malloc(logSize);

    if (log)
    {
      // Get the log
      result = clGetProgramBuildInfo(program, deviceID, CL_PROGRAM_BUILD_LOG, logSize, log, NULL);

      // Print the log
      puts(log);
      free(log);
    }
#endif

    goto epilogue;
  }

  if (result != CL_SUCCESS)
    goto epilogue;

  decodeKernel = clCreateKernel(program, "decode_ans", &result);

  if (result != CL_SUCCESS || decodeKernel == NULL)
    goto epilogue;

  initialized = true;

epilogue:
  if (!success)
  {
    // TODO: Destroy all things under heaven.
  }

  free(pPlatforms);

  return success;
}

static inline void rans_opencl_DecInit_single(RansState* r, uint8_t** pptr)
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

bool rans_opencl_uncompress_O0_32x16(unsigned char *in, unsigned int in_size, unsigned int out_size)
{
  (void)in_size;

  unsigned char *cp = in + 4;
  int i, j, x, y, out_sz, rle;

  uint8_t  ssym[TOTFREQ + 64]; // faster to use 16-bit on clang

  uint32_t s3[TOTFREQ]; // For TF_SHIFT <= 12

  out_sz = ((in[0]) << 0) | ((in[1]) << 8) | ((in[2]) << 16) | ((in[3]) << 24);

  if ((int64_t)out_sz > (int64_t)out_size)
    return false;

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

  if (x >= TOTFREQ)
    return false;

  int z;

  RansState R[NX];
  for (z = 0; z < NX; z++)
    rans_opencl_DecInit_single(&R[z], &cp);

  const uint32_t symbolCount = (out_sz&~(NX - 1));
  const uint32_t overhang = out_sz & (NX - 1);
  const uint32_t zero = 0;

  cl_int result = CL_SUCCESS;

  if (CL_SUCCESS != (result = clEnqueueWriteBuffer(commandQueue, gpu_in_buffer, CL_FALSE, 0, in_size - (cp - in), cp, 0, NULL, NULL)))
    return false;

  if (CL_SUCCESS != (result = clEnqueueWriteBuffer(commandQueue, gpu_in_buffer_R, CL_FALSE, 0, NX * sizeof(uint32_t), R, 0, NULL, NULL)))
    return false;

  if (CL_SUCCESS != (result = clEnqueueWriteBuffer(commandQueue, gpu_in_buffer_S3, CL_FALSE, 0, TOTFREQ * sizeof(uint32_t), s3, 0, NULL, NULL)))
    return false;

  if (CL_SUCCESS != (result = clEnqueueWriteBuffer(commandQueue, gpu_in_buffer_SSYM, CL_FALSE, 0, TOTFREQ + 64, ssym, 0, NULL, NULL)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 0, sizeof(cl_mem), (void *)&gpu_in_buffer)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 1, sizeof(cl_mem), (void *)&gpu_out_buffer)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 2, sizeof(uint32_t), (void *)&zero)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 3, sizeof(uint32_t), (void *)&symbolCount)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 4, sizeof(uint32_t), (void *)&overhang)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 5, sizeof(cl_mem), (void *)&gpu_in_buffer_S3)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 6, sizeof(cl_mem), (void *)&gpu_in_buffer_R)))
    return false;

  if (CL_SUCCESS != (result = clSetKernelArg(decodeKernel, 7, sizeof(cl_mem), (void *)&gpu_in_buffer_SSYM)))
    return false;

  size_t globalSize[2] = { NX, 1 };
  size_t localSize[2] = { NX, 1 };

  if (CL_SUCCESS != (result = clEnqueueNDRangeKernel(commandQueue, decodeKernel, 2, NULL, globalSize, localSize, 0, NULL, NULL)))
    return false;

  return true;
}

bool rans_opencl_download_buffer(uint8_t *pBuffer, const size_t size)
{
  cl_int result = CL_SUCCESS;

  if (CL_SUCCESS != (result = clEnqueueReadBuffer(commandQueue, gpu_out_buffer, CL_TRUE, 0, size, pBuffer, 0, NULL, NULL)))
    return false;

  return true;
}

void rans_opencl_destroy()
{
  // TODO: Destroy all things under heaven.
}
