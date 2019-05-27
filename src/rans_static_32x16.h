#ifndef rans_static_32x16_h__
#define rans_static_32x16_h__

// The SIMD & Non SIMD Functions should produce equivalent results.

#ifdef __cplusplus
extern "C"
{
#endif
  // SIMD (AVX2 required):
  unsigned int rans_compress_bound_32x16_AVX2(unsigned int size, int order, int *tab);
  unsigned char *rans_compress_to_32x16_AVX2(unsigned char *in, unsigned int in_size, unsigned char *out, unsigned int *out_size, int order);
  unsigned char *rans_uncompress_to_32x16_AVX2(unsigned char *in, unsigned int in_size, unsigned char *out, unsigned int *out_size, int order);

  // No SIMD:
  unsigned int rans_compress_bound_32x16_NoSimd(unsigned int size, int order, int *tab);
  unsigned char *rans_compress_to_32x16_NoSimd(unsigned char *in, unsigned int in_size, unsigned char *out, unsigned int *out_size, int order);
  unsigned char *rans_uncompress_to_32x16_NoSimd(unsigned char *in, unsigned int in_size, unsigned char *out, unsigned int *out_size, int order);
#ifdef __cplusplus
}
#endif

#endif // rans_static_32x16_h__
