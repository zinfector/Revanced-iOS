#include "Events.h"
#include "Operations.h"
#include "app/revanced/extension/youtube/patches/playback/speed/RememberPlaybackSpeedPatch.h"
void RVST_RememberPlaybackSpeedPatch_newVideoStarted(void) { RVSTRequireEventContext(); RVSTPolicyRememberPlaybackSpeedPatch_newVideoStartedWithRVSTContextVideoInformation_PlaybackController_(nil); }
void RVST_RememberPlaybackSpeedPatch_userSelectedPlaybackSpeed(float playbackSpeed) { RVSTRequireEventContext(); RVSTPolicyRememberPlaybackSpeedPatch_userSelectedPlaybackSpeedWithFloat_(playbackSpeed); }
float RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride(void) { RVSTRequireEventContext(); return RVSTPolicyRememberPlaybackSpeedPatch_getPlaybackSpeedOverride(); }
