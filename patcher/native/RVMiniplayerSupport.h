// SPDX-License-Identifier: MIT
#pragma once
#include <math.h>
static double RVMiniplayerDimension(double native,double configured,double width,double height) {
    if (!isfinite(native) || native<=0 || !isfinite(configured) || configured==0 || !isfinite(width) || !isfinite(height)) return native;
    double maximum=fmin(480,fmin(width,height)-32);
    if (maximum<170 || configured<170 || configured>480) return native;
    return fmin(configured,maximum);
}
static double RVMiniplayerAlpha(double native,double configured) {
    return isfinite(native) && isfinite(configured) && configured>=0 && configured<=1 ? native*configured : native;
}
