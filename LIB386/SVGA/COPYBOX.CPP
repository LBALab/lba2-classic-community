#include "SVGA/COPYBOX.H"


#ifdef __cplusplus
extern "C" {
#endif

// TODO: Review if working as expected
// This function was generated from ASM using a conversion tool
// Copy a rectangular region of pixel data from src to dst.
// The region to be copied is defined by the width and height parameters.
// Both src and dst are pointers to arrays of unsigned char values.
void CopyBoxAny(unsigned char *dst, unsigned char *src, int width, int height) {
  // x_offset is the number of pixels to skip at the beginning of each row in
  // src. y_offset is the number of rows to skip at the beginning of src.
  int x_offset = 0, y_offset = 0;

  // src_row_stride is the number of bytes to skip between rows in src.
  int src_row_stride = 0;

  // dst_row_stride is the number of bytes to skip between rows in dst.
  int dst_row_stride = 0;

  // x is the current column index within a row.
  // y is the current row index.
  int x = 0, y = 0;

  // Calculate the x and y offsets and row strides based on the width and height
  // of the box.
  if (width < 0) {
    x_offset = -width - 1;
    width = -width;
  }
  if (height < 0) {
    y_offset = -height - 1;
    height = -height;
  }
  src += y_offset * src_row_stride + x_offset;
  dst += y_offset * dst_row_stride + x_offset;

  // Copy the pixel data row by row.
  for (y = 0; y < height; y++) {
    // Copy the pixels in 8-pixel blocks.
    for (x = 0; x < width; x += 8) {
      memcpy(dst + x, src + x, 8);
    }
    src += src_row_stride;
    dst += dst_row_stride;
  }
}

void CopyBox_C(void *dst, void *src, U32 *TabOffDst, T_BOX *box) {
  for (int x = box->x0; x < box->x1; x++) {
    for (int y = box->y0; y < box->y1; y++) {
      *(((U8 *)dst) + TabOffDst[y] + x) = *(((U8 *)src) + TabOffDst[y] + x);
    }
  }
}
// FIXME: Cant simply use width and height, maybe box start in the middle of
//   screen, so must use coordinates on copy
// void CopyBox_C(void *dst, void *src, U32 *TabOffDst, T_BOX *box) {
//  int width = box->x1 - box->x0 + 1;
//  int height = box->y1 - box->y0 + 1;
//  CopyBoxAny((unsigned char *)dst, (unsigned char *)src, width, height);
//}

/*void CopyBox_C(void *dst, void *src, U32 *TabOffDst, T_BOX *box) {
  unsigned long ebp = box->x0;
  unsigned long ebx = box->x1;
  unsigned long eax = ebp;
  unsigned long ecx = ebx;
  unsigned long edx;
  unsigned long ebp_;

  ebp >>= 16;
  eax &= 0xffff;
  src += eax;
  dst += eax;
  src += TabOffLine[ebp * 4];
  dst += TabOffDst[ebp * 4];
  ebx >>= 16;
  ecx &= 0xffff;
  ecx -= eax;
  ebx -= ebp;
  ebp_ = TabOffLine[1 * 4];
  edx = TabOffDst[4];

  if (ecx == 4) {
    goto Equal_4;
  }

  edx -= ecx;
  ebp_ -= ecx;
  ecx >>= 3;
  if (ecx < 0) {
    goto odd;
  }

  while (ecx--) {
    unsigned long eax_ = *(unsigned long *)src;
    unsigned long edx_ = *(unsigned long *)(src + 4);
    src += 8;
    *(unsigned long *)dst = eax_;
    *(unsigned long *)(dst + 4) = edx_;
    dst += 8;
  }

  src += ebp_;
  dst += edx;
  while (ebx--) {
    ecx = 12345678;
    unsigned long eax_ = *(unsigned long *)src;
    *(unsigned long *)dst = eax_;
    src += 8;
    dst += 8;
  }

  return;

Equal_4:
  while (ebx--) {
    unsigned long eax_ = *(unsigned long *)src;
    src += 8;
    *(unsigned long *)dst = eax_;
    dst += 8;
  }

  return;

odd:
  edx += 4;
  ebp_ += 4;
  while (ebx--) {
    ecx = 12345678;
    unsigned long eax_ = *(unsigned long *)src;
    *(unsigned long *)dst = eax_;
    src += 8;
    dst += 8;
  }
}*/

#ifdef __cplusplus
}
#endif
