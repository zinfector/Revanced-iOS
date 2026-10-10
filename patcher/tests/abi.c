#include "../native/RVABI.h"
#include <stdio.h>
int main(int argc,char **argv) {
    for (int i=1;i<argc;i++) {
        char out[4097];puts(RVABINormalize(argv[i],out,sizeof(out)) ? out : "INVALID");
    }
    return 0;
}
