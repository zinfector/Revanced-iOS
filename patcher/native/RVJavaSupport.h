#ifndef RV_JAVA_SUPPORT_H
#define RV_JAVA_SUPPORT_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct RVJContext RVJContext;
typedef struct { const int8_t *data; int32_t length; } RVJBytes;
typedef struct { int32_t *data; int32_t length; } RVJInts;
static inline int32_t rvj_add(int32_t a,int32_t b) { return (int32_t)((uint32_t)a+(uint32_t)b); }
static inline int32_t rvj_sub(int32_t a,int32_t b) { return (int32_t)((uint32_t)a-(uint32_t)b); }
static inline int32_t rvj_mul(int32_t a,int32_t b) { return (int32_t)((uint32_t)a*(uint32_t)b); }
// Implemented after the generated context definition, which owns the error state.
int32_t rvj_byte_at(RVJContext *ctx,RVJBytes value,int32_t index);
int32_t rvj_int_at(RVJContext *ctx,RVJInts value,int32_t index);
void rvj_int_set(RVJContext *ctx,RVJInts value,int32_t index,int32_t item);
RVJInts rvj_ints_new(RVJContext *ctx,int32_t length);
void rvj_ints_free(RVJInts value);
#endif
