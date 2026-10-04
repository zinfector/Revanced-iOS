// SPDX-License-Identifier: MIT
// Shared interval validation, merging and marker geometry; also executed by host regression tests.
#ifndef RV_INTERVALS_H
#define RV_INTERVALS_H
#include <math.h>
#include <stddef.h>
typedef struct { double start,end; } RVInterval;
static inline int RVIntervalValid(RVInterval value,int point,double minimum) {
    return isfinite(value.start) && isfinite(value.end) && isfinite(minimum) && minimum>=0 && value.start>=0 &&
        (point ? value.end>=value.start : value.end>value.start && value.end-value.start>=minimum);
}
// Input must be sorted by start. Invalid input/capacity returns zero without publishing a partial list.
static inline size_t RVMergeIntervals(const RVInterval *input,size_t count,RVInterval *output,size_t capacity) {
    if ((!input && count) || (!output && count) || capacity<count) return 0;
    for (size_t i=0;i<count;i++) if (!RVIntervalValid(input[i],0,0) || (i && input[i].start<input[i-1].start)) return 0;
    size_t length=0;
    for (size_t i=0;i<count;i++) {
        if (length && input[i].start<=output[length-1].end) output[length-1].end=fmax(output[length-1].end,input[i].end);
        else output[length++]=input[i];
    }
    return length;
}
static inline int RVMarkerBounds(RVInterval interval,double duration,double width,double *left,double *size) {
    if (!left || !size || !RVIntervalValid(interval,1,0) || !isfinite(duration) || duration<=0 || !isfinite(width) || width<=0 || interval.start>duration) return 0;
    *left=fmin(width,interval.start/duration*width);
    *size=fmin(width-*left,fmax(2,(fmin(duration,interval.end)-interval.start)/duration*width));
    return *size>0;
}
#endif
