#include "RVSourceKernels.h"
int32_t rvj_byte_at(RVJContext *ctx,RVJBytes value,int32_t index) {
    if(index<0 || index>=value.length || !value.data) { ctx->error=1;return 0; }
    return value.data[index];
}
int32_t rvj_int_at(RVJContext *ctx,RVJInts value,int32_t index) {
    if(index<0 || index>=value.length || !value.data) { ctx->error=1;return 0; }
    return value.data[index];
}
void rvj_int_set(RVJContext *ctx,RVJInts value,int32_t index,int32_t item) {
    if(index<0 || index>=value.length || !value.data) { ctx->error=1;return; }
    value.data[index]=item;
}
RVJInts rvj_ints_new(RVJContext *ctx,int32_t length) {
    if(length<0 || length>1024*1024) { ctx->error=2;return (RVJInts){0}; }
    int32_t *data=calloc(length ? (size_t)length : 1,sizeof(int32_t));
    if(!data) { ctx->error=3;return (RVJInts){0}; }
    return (RVJInts){data,length};
}
void rvj_ints_free(RVJInts value) { free(value.data); }
