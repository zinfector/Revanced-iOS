#include "../native/RVIntervals.h"
#include "../native/RVImageHeader.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"interval check failed on line %d\n",__LINE__);return 1; } } while (0)
int main(void) {
    RVInterval ranges[]={{0,4},{2,8},{8,10},{12,14},{12,13},{20,30}},out[6];
    CHECK(RVMergeIntervals(ranges,6,out,6)==3);
    CHECK(out[0].start==0 && out[0].end==10 && out[1].start==12 && out[1].end==14 && out[2].end==30);
    CHECK(RVMergeIntervals(ranges,6,out,2)==0);
    RVInterval unordered[]={{5,7},{0,4}};CHECK(RVMergeIntervals(unordered,2,out,6)==0);
    RVInterval invalid[]={{0,NAN}};CHECK(RVMergeIntervals(invalid,1,out,6)==0);
    CHECK(!RVIntervalValid((RVInterval){-1,2},0,0));CHECK(!RVIntervalValid((RVInterval){2,2},0,0));
    CHECK(RVIntervalValid((RVInterval){2,2},1,0));CHECK(!RVIntervalValid((RVInterval){0,INFINITY},0,0));
    CHECK(!RVIntervalValid((RVInterval){0,.1},0,.2));CHECK(RVIntervalValid((RVInterval){0,.2},0,.2));
    double left,size;
    CHECK(RVMarkerBounds((RVInterval){25,50},100,200,&left,&size) && left==50 && size==50);
    CHECK(RVMarkerBounds((RVInterval){90,200},100,200,&left,&size) && left==180 && size==20);
    CHECK(RVMarkerBounds((RVInterval){50,50},100,200,&left,&size) && left==100 && size==2);
    CHECK(!RVMarkerBounds((RVInterval){101,110},100,200,&left,&size));
    CHECK(!RVMarkerBounds((RVInterval){0,10},0,200,&left,&size));
    CHECK(!RVMarkerBounds((RVInterval){0,10},100,NAN,&left,&size));
    const uint8_t jpeg[]={0xff,0xd8,0xff},png[]={0x89,'P','N','G','\r','\n',0x1a,'\n'},webp[]="RIFF0000WEBP";
    CHECK(RVImagePrefixValid(jpeg,3));CHECK(!RVImagePrefixValid(jpeg,2));
    CHECK(RVImagePrefixValid(png,8));CHECK(!RVImagePrefixValid(png,7));
    CHECK(RVImagePrefixValid(webp,12));CHECK(!RVImagePrefixValid(webp,11));CHECK(!RVImagePrefixValid((const uint8_t *)"not an image",12));
    CHECK(RVImageResponseValid(206,"image/jpeg",32));CHECK(RVImageResponseValid(200,"image/webp",-1));
    CHECK(!RVImageResponseValid(404,"image/jpeg",32));CHECK(!RVImageResponseValid(204,"image/jpeg",0));
    CHECK(!RVImageResponseValid(200,"text/html",100));CHECK(!RVImageResponseValid(200,"image/jpeg",6*1024*1024));
    puts("Shared native interval and marker geometry checks passed.");return 0;
}
