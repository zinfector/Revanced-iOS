#pragma once
#include <stdbool.h>
#include <stdint.h>
void RVST_RememberPlaybackSpeedPatch_newVideoStarted(void);
void RVST_RememberPlaybackSpeedPatch_userSelectedPlaybackSpeed(float playbackSpeed);
float RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride(void);
