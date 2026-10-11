package rv.batch;
import app.revanced.extension.youtube.patches.*;
import app.revanced.extension.youtube.patches.playback.speed.*;
import app.revanced.extension.youtube.patches.litho.*;
import app.revanced.extension.youtube.shared.*;
import app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController;
import app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike;
import app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData;
import app.revanced.extension.shared.patches.litho.*;
import android.text.*;

/** Placement API. The host must supply a reviewed owner and current video identity. */
public final class Events {
 public static boolean showVideoAds(){return VideoAdsPatch.shouldShowAds();}
 public static boolean allowBackground(boolean original){return BackgroundPlaybackPatch.isBackgroundPlaybackAllowed(original);}
 public static boolean allowShortsBackground(boolean original){return BackgroundPlaybackPatch.isBackgroundShortsPlaybackAllowed(original);}
 public static boolean hideInfoCards(){return HideInfoCardsPatch.hideInfoCardsMethodCall();}
 public static boolean hideEndScreenCards(){return HideEndScreenCardsPatch.hideEndScreenCards();}
 public static void playerState(String type,boolean shorts){PlayerType.setFromString(type);ShortsPlayerState.setOpen(shorts);}
 public static void beginVideo(String videoId){RememberPlaybackSpeedPatch.newVideoStarted(null);SegmentPlaybackController.initialize(null);SegmentPlaybackController.setCurrentVideoId(videoId);ReturnYouTubeDislikePatch.newVideoLoaded(videoId);}
 public static void timeChanged(long millis){SegmentPlaybackController.setVideoTime(millis);}
 public static void rateSelected(float rate){RememberPlaybackSpeedPatch.userSelectedPlaybackSpeed(rate);}
 public static float rateOverride(){return RememberPlaybackSpeedPatch.getPlaybackSpeedOverride();}
 public static void vote(int value){ReturnYouTubeDislikePatch.sendVote(value);}
 public static long dislikes(String videoId,long timeout){RYDVoteData data=ReturnYouTubeDislike.getFetchForVideoId(videoId).getFetchData(timeout);return data==null?-1:data.getDislikeCount();}
 public static long likes(String videoId,long timeout){RYDVoteData data=ReturnYouTubeDislike.getFetchForVideoId(videoId).getFetchData(timeout);return data==null?-1:data.getLikeCount();}
 public static CharSequence voteText(String videoId,CharSequence original,boolean segmented){return ReturnYouTubeDislike.getFetchForVideoId(videoId).getDislikesSpanForRegularVideo(new SpannableString(original),segmented,false);}
 public static boolean filter(String family,String identifier,String accessibility,String path,byte[] buffer){
  Filter f=family.equals("ads")?Filters.ads:family.equals("shorts")?Filters.shorts:family.equals("ryd")?Filters.ryd:null;
  if(f==null)throw new IllegalArgumentException("Unknown filter family");
  for(FilterGroup.StringFilterGroup group:f.identifierCallbacks){FilterGroup.FilterGroupResult result=group.check(identifier);if(result.isFiltered()&&f.isFiltered(identifier,accessibility,path,buffer,group,Filter.FilterContentType.IDENTIFIER,result.getMatchedIndex()))return true;}
  for(FilterGroup.StringFilterGroup group:f.pathCallbacks){FilterGroup.FilterGroupResult result=group.check(path);if(result.isFiltered()&&f.isFiltered(identifier,accessibility,path,buffer,group,Filter.FilterContentType.PATH,result.getMatchedIndex()))return true;}
  return false;
 }
 private static final class Filters {static final Filter ads=new AdsFilter(),shorts=new ShortsFilter(),ryd=new ReturnYouTubeDislikeFilter();}
}
