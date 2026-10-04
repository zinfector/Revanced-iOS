// SPDX-License-Identifier: MIT
#ifndef RV_IMAGE_HEADER_H
#define RV_IMAGE_HEADER_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static inline int RVImageResponseValid(int status,const char *mime,long long expected) {
    return (status==200 || status==206) && mime && !strncmp(mime,"image/",6) && expected<=5*1024*1024;
}
static inline int RVImagePrefixValid(const uint8_t *p,size_t n) {
    if (!p) return 0;
    return (n>=3 && p[0]==0xff && p[1]==0xd8 && p[2]==0xff) ||
        (n>=8 && !memcmp(p,"\x89PNG\r\n\x1a\n",8)) ||
        (n>=12 && !memcmp(p,"RIFF",4) && !memcmp(p+8,"WEBP",4));
}
#endif
