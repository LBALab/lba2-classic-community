#pragma once

#include <stdint.h>

//------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------
typedef uint8_t  U8;
typedef int8_t   S8;
typedef uint16_t U16;
typedef int16_t  S16;
typedef uint32_t U32;
typedef int32_t  S32;
typedef uint64_t U64;
typedef int64_t  S64;
//typedef int32_t  LONG;

//------------------------------------------------------------------------------
typedef void  *PTR_VOID;
typedef U64   *PTR_U64;
typedef S64   *PTR_S64;
typedef U32   *PTR_U32;
typedef S32   *PTR_S32;
typedef U16   *PTR_U16;
typedef S16   *PTR_S16;
typedef U8    *PTR_U8;
typedef S8    *PTR_S8;

typedef void(VOID_FUNC)();
typedef VOID_FUNC *PTR_VOID_FUNC;

//------------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct STR_T_BOX {
  S16 x0;
  S16 y0;
  S16 x1;
  S16 y1;
  struct STR_T_BOX *pBoxNext;
} T_BOX;
#pragma pack(pop)

typedef void (BOX_FUNC)(T_BOX *pbox);

//------------------------------------------------------------------------------
#define AND &&
#define OR ||

#define TRUE 1
#define FALSE 0

// =============================================================================
#ifdef __cplusplus
}
#endif
