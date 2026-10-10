// Bounded Objective-C encoding parser, shared by runtime guards and C tests.
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
static bool RVABIToken(const char *s,size_t n,size_t *p,unsigned depth) {
    if (depth>64) return false;
    while (*p<n && strchr("rnNoORV",s[*p])) ++*p;
    if (*p>=n) return false;
    char c=s[(*p)++];
    if (c=='^') return RVABIToken(s,n,p,depth+1);
    if (c=='{' || c=='[' || c=='(') {
        char close=c=='{' ? '}' : c=='[' ? ']' : ')';
        if (c=='[') {
            size_t start=*p;while (*p<n && isdigit((unsigned char)s[*p])) ++*p;
            if (*p==start || !RVABIToken(s,n,p,depth+1)) return false;
        } else {
            while (*p<n && s[*p]!='=' && s[*p]!=close) ++*p;
            if (*p<n && s[*p]=='=') {
                ++*p;
                while (*p<n && s[*p]!=close) {
                    if (s[*p]=='"') {
                        ++*p;while (*p<n && s[*p]!='"') ++*p;
                        if (*p>=n) return false;++*p;
                    } else if (!RVABIToken(s,n,p,depth+1)) return false;
                }
            }
        }
        if (*p>=n || s[*p]!=close) return false;++*p;return true;
    }
    if (c=='b') {
        size_t start=*p;while (*p<n && isdigit((unsigned char)s[*p])) ++*p;
        return *p!=start;
    }
    if (c=='@') {
        if (*p<n && s[*p]=='?') ++*p;
        else if (*p<n && s[*p]=='"') {
            ++*p;while (*p<n && s[*p]!='"') ++*p;
            if (*p>=n) return false;++*p;
        }
        return true;
    }
    return strchr("cislqCISLQfdDBv*#:?",c)!=NULL;
}
static bool RVABINormalize(const char *s,char *out,size_t capacity) {
    if (!s || !out || !capacity) return false;
    size_t n=strnlen(s,4097),p=0,q=0;
    if (!n || n>4096) return false;
    while (p<n) {
        size_t start=p;if (!RVABIToken(s,n,&p,0)) return false;
        if (p-start>=capacity-q) return false;
        memcpy(out+q,s+start,p-start);q+=p-start;
        size_t offset=p;
        if (p<n && (s[p]=='+' || s[p]=='-')) ++p;
        size_t digits=p;while (p<n && isdigit((unsigned char)s[p])) ++p;
        if (p==digits) p=offset;
    }
    out[q]=0;return true;
}
