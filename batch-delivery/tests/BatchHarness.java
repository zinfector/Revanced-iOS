package rv.batch.test;
import rv.batch.*;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.*;
import app.revanced.extension.youtube.patches.*;
import app.revanced.extension.youtube.patches.playback.speed.*;
import app.revanced.extension.youtube.sponsorblock.*;
import app.revanced.extension.youtube.sponsorblock.objects.*;
import app.revanced.extension.youtube.returnyoutubedislike.*;
import app.revanced.extension.youtube.returnyoutubedislike.requests.*;
import app.revanced.extension.shared.patches.litho.*;
import android.text.*;
import android.text.style.*;
import android.graphics.*;
import android.content.Context;
import android.view.*;
import org.json.*;
import java.util.*;
import java.nio.charset.StandardCharsets;
import java.lang.reflect.Method;

/** Deliberately fake operation environment shared by the source oracle and IR output. */
public final class BatchHarness {
 private static final StringBuilder trace=new StringBuilder();
 static void line(String key,Object value){trace.append(key).append('|').append(value).append('\n');}
 static void check(boolean value,String message){if(!value)throw new AssertionError(message);}
 public static final class Ops implements Platform.Operations {
  final Map<String,Object> values=new HashMap<>();final List<Job> jobs=new ArrayList<>();final List<String> effects=new ArrayList<>();
  long now=1_000_000,serial;boolean main=true,seekSuccess=true;int rydStatus=200,requests;
  final Context context=new Context();
  final class Job {final Runnable callback;final long deadline,id;final boolean main;Job(Runnable r,long d,boolean m){callback=r;deadline=now+Math.max(0,d);main=m;id=serial++;}}
  public Object read(String key,Object fallback){return values.getOrDefault(key,fallback);}public void write(String key,Object value){values.put(key,value);}
  public long wallMillis(){return now;}public void post(Runnable r,long d,boolean m){jobs.add(new Job(r,d,m));}
  void drain(){int limit=200;while(--limit>0){Job next=null;for(Job j:jobs)if(j.deadline<=now&&(next==null||j.deadline<next.deadline||j.deadline==next.deadline&&j.id<next.id))next=j;if(next==null)return;jobs.remove(next);boolean before=main;main=next.main;try{next.callback.run();}finally{main=before;}}throw new AssertionError("Callback cycle");}
  void advance(long ms){now+=ms;drain();}
  public String resource(String key,Object[] args){return key+ (args.length==0?"":Arrays.toString(args));}
  public Object invoke(String op,Object[] a){
   if(op.equals("thread.is_main"))return main;if(op.equals("network.connected"))return true;if(op.equals("locale"))return "en-US";
   if(op.equals("theme.dark"))return false;if(op.equals("theme.background"))return 0xffffffff;if(op.equals("theme.foreground"))return 0xff000000;
   if(op.startsWith("context."))return context;if(op.equals("application.version"))return "21.39.4";
   if(op.equals("log"))return null;if(op.equals("resource.id"))return a[1].hashCode();if(op.equals("resource.integer"))return 200;
   if(op.equals("text.measure"))return ((String)a[0]).length()*8f;
   if(op.equals("text.font_metrics")){Paint.FontMetricsInt f=new Paint.FontMetricsInt();f.ascent=-12;f.top=-14;f.descent=3;f.bottom=4;return f;}
   if(op.equals("format.number")){java.text.NumberFormat f;String style=(String)a[2];Locale locale=Locale.forLanguageTag((String)a[1]);
    // Recorded formatting fixture: tested count inputs are below compact thresholds.
    if(style.startsWith("compact")&&((Number)a[0]).doubleValue()>=1000)throw new AssertionError("Unrecorded compact formatting input");
    f=style.equals("percent")?java.text.NumberFormat.getPercentInstance(locale):java.text.NumberFormat.getNumberInstance(locale);
    if((Integer)a[3]>=0)f.setMinimumFractionDigits((Integer)a[3]);if((Integer)a[4]>=0)f.setMaximumFractionDigits((Integer)a[4]);return f.format(a[0]);}
   if(op.equals("http.request")){
    Network.Request q=(Network.Request)a[0];requests++;effects.add("http:"+q.method+":"+q.url);
    int status=200;String body="{}";
    if(q.url.contains("returnyoutubedislike")){status=rydStatus;String id="";for(String item:q.url.substring(q.url.indexOf('?')+1).split("&"))if(item.startsWith("videoId="))id=item.substring(8);
     body="{\"id\":\""+id+"\",\"viewCount\":1000,\"likes\":100,\"dislikes\":"+(id.equals("videoB")?25:10)+"}";
    }else if(q.url.contains("skipSegments")){body="[{\"segment\":[5,10],\"UUID\":\"s1\",\"category\":\"sponsor\",\"locked\":0},{\"segment\":[8,15],\"UUID\":\"s2\",\"category\":\"sponsor\",\"locked\":0},{\"segment\":[3,4],\"UUID\":\"unknown\",\"category\":\"unknown\",\"locked\":0}]";}
    return new Network.Response(status,body.getBytes(StandardCharsets.UTF_8),Collections.emptyMap());
   }
   if(op.equals("http.disconnect"))return null;
   if(op.equals("player.seek_ms")){effects.add("seek:"+a[0]+":"+seekSuccess);if(seekSuccess)values.put("player.position_ms",a[0]);return seekSuccess;}
   if(op.equals("player.set_rate")){effects.add("rate:"+a[0]);values.put("player.rate",a[0]);return null;}
   if(op.equals("dialog.feature"))return true;if(op.equals("canvas.save"))return 1;
   if(op.startsWith("view.")||op.startsWith("role.")||op.startsWith("roles.")||op.startsWith("dialog.")||op.startsWith("window.")||op.startsWith("canvas.")||op.startsWith("drawable.")||op.equals("toast")){effects.add(op);return null;}
   throw new UnsupportedOperationException(op);
  }
 }
 public static String run()throws Exception {
  Locale.setDefault(Locale.US);Ops ops=new Ops();Platform.operations=ops;
  ops.values.put("player.position_ms",0L);ops.values.put("player.duration_ms",120000L);ops.values.put("player.rate",1f);
  Settings.SB_ENABLED.save(true);Settings.RYD_ENABLED.save(true);Settings.SB_TOAST_ON_SKIP.save(false);
  Settings.SB_PRIVATE_USER_ID.save("source-oracle-private-id-012345678901234567890");
  line("ads.default",Events.showVideoAds());check(!Events.showVideoAds(),"Ads setting default");
  for(PlayerType type:PlayerType.values()){Events.playerState(type.name(),false);line("background."+type,Events.allowBackground(false));line("background.original."+type,Events.allowBackground(true));}
  Events.playerState("WATCH_WHILE_MAXIMIZED",true);check(!Events.allowBackground(false),"Short background blocked");line("background.short",Events.allowBackground(false));Events.playerState("WATCH_WHILE_MAXIMIZED",false);
  Settings.DISABLE_SHORTS_BACKGROUND_PLAYBACK.save(true);line("short-background.disabled",Events.allowShortsBackground(true));Settings.DISABLE_SHORTS_BACKGROUND_PLAYBACK.save(false);line("short-background.enabled",Events.allowShortsBackground(false));
  for(boolean enabled:new boolean[]{false,true}){Settings.HIDE_INFO_CARDS.save(enabled);Settings.HIDE_END_SCREEN_CARDS.save(enabled);line("cleanup."+enabled,Events.hideInfoCards()+":"+Events.hideEndScreenCards());
   View v=new View(ops.context);HideEndScreenCardsPatch.hideEndScreenCardView(v);line("cleanup.visibility."+enabled,v.getVisibility());}
  FilterGroup.StringFilterGroup pattern=new FilterGroup.StringFilterGroup(Settings.HIDE_INFO_CARDS,"abcdef","bc");Settings.HIDE_INFO_CARDS.save(true);line("filter.match",pattern.check("--abcdef--").getMatchedIndex());Settings.HIDE_INFO_CARDS.save(false);line("filter.disabled",pattern.check("--abcdef--").isFiltered());
  Settings.HIDE_GENERAL_ADS.save(true);check(Events.filter("ads","carousel_ad","","carousel_ad",new byte[0]),"Source ad filter match");line("filter.ads",Events.filter("ads","carousel_ad","","carousel_ad",new byte[0]));
  Settings.HIDE_SHORTS_HOME.save(true);line("filter.shorts",Events.filter("shorts","shorts_shelf","","shorts_shelf",new byte[0]));
  String[] voteInputs={"{\"id\":\"v\",\"viewCount\":1000,\"likes\":100,\"dislikes\":25}","{\"id\":\"v\",\"viewCount\":1000,\"likes\":0,\"dislikes\":100,\"rawLikes\":30,\"rawDislikes\":10}","{\"id\":\"v\",\"viewCount\":0,\"likes\":0,\"dislikes\":0}"};
  for(int i=0;i<voteInputs.length;i++){RYDVoteData data=new RYDVoteData(new JSONObject(voteInputs[i]));for(ReturnYouTubeDislike.Vote vote:ReturnYouTubeDislike.Vote.values()){data.updateUsingVote(vote);line("ryd.data."+i+"."+vote,data.getLikeCount()+":"+data.getDislikeCount()+":"+Float.floatToRawIntBits(data.getDislikePercentage()));}}
  boolean rejected=false;try{new RYDVoteData(new JSONObject("{\"id\":\"x\",\"viewCount\":-1,\"likes\":0,\"dislikes\":0}"));}catch(JSONException e){rejected=true;}check(rejected,"Reject negative counts");line("ryd.reject-negative",rejected);
  ReturnYouTubeDislike fetch=ReturnYouTubeDislike.getFetchForVideoId("videoA");line("ryd.pending",fetch.fetchCompleted());ops.drain();check(fetch.getFetchData(0).getDislikeCount()==10,"A dislikes");
  line("ryd.fetchA",fetch.getFetchData(0).getDislikeCount());line("ryd.cache-hit",fetch==ReturnYouTubeDislike.getFetchForVideoId("videoA"));
  ReturnYouTubeDislike b=ReturnYouTubeDislike.getFetchForVideoId("videoB");ops.drain();line("ryd.fetchB",b.getFetchData(0).getDislikeCount());check(b.getFetchData(0).getDislikeCount()==25,"Video owner identity");
  SpannableString likes=new SpannableString("100");likes.setSpan(new ForegroundColorSpan(0xff112233),0,3,33);
  Spanned unsegmented=fetch.getDislikesSpanForRegularVideo(likes,false,false);line("ryd.dislike-text",unsegmented.toString());line("ryd.color",unsegmented.getSpans(0,unsegmented.length(),ForegroundColorSpan.class)[0].getForegroundColor());
  ops.advance(420001);line("ryd.cache-expired",fetch!=ReturnYouTubeDislike.getFetchForVideoId("videoA"));ops.drain();
  Method solve=ReturnYouTubeDislikeAPI.class.getDeclaredMethod("solvePuzzle",String.class,int.class);solve.setAccessible(true);line("ryd.pow",solve.invoke(null,"AAAAAAAAAAAAAAAAAAAAAA==",8));
  ops.main=false;ops.rydStatus=429;ReturnYouTubeDislikeAPI.fetchVotes("limited");int before=ops.requests;ReturnYouTubeDislikeAPI.fetchVotes("limited2");line("ryd.rate-limit-blocked",before==ops.requests);check(before==ops.requests,"429 cooldown blocks request");ops.main=true;ops.rydStatus=200;ReturnYouTubeDislikeAPI.resetRateLimits();
  SponsorSegment one=new SponsorSegment(SegmentCategory.SPONSOR,"a",5000,10000,false),overlap=new SponsorSegment(SegmentCategory.SPONSOR,"b",8000,15000,false);
  line("sb.bounds",one.containsTime(5000)+":"+one.containsTime(9999)+":"+one.containsTime(10000));line("sb.overlap",one.intersectsRange(overlap.getUndoRange()));line("sb.undo",one.getUndoRange().getLower()+":"+one.getUndoRange().getUpper());
  SegmentCategory.SPONSOR.behaviour=CategoryBehaviour.SKIP_AUTOMATICALLY_ONCE;line("sb.once-before",one.shouldAutoSkip());one.didAutoSkipped=true;line("sb.once-after",one.shouldAutoSkip());
  SegmentPlaybackController.initialize(null);SegmentPlaybackController.setCurrentVideoId("old");SegmentPlaybackController.setCurrentVideoId("current");ops.drain();line("sb.downloaded",SegmentPlaybackController.videoHasSegments());check(SegmentPlaybackController.videoHasSegments(),"Sponsor segments parsed");
  ops.values.put("player.position_ms",5000L);SegmentPlaybackController.setVideoTime(5000);ops.drain();line("sb.position-after-skip",ops.values.get("player.position_ms"));check(ops.values.get("player.position_ms").equals(10000L),"First segment source skip");
  ops.values.put("player.position_ms",10000L);SegmentPlaybackController.setVideoTime(10000);ops.drain();line("sb.overlap-second-skip",ops.values.get("player.position_ms"));check(ops.values.get("player.position_ms").equals(15000L),"Overlapping second segment source skip");
  List<String> seeks=new ArrayList<>();for(String effect:ops.effects)if(effect.startsWith("seek:"))seeks.add(effect);line("sb.effect-seeks",seeks);
  Settings.SB_VIDEO_LENGTH_WITHOUT_SEGMENTS.save(true);line("sb.duration",SegmentPlaybackController.appendTimeWithoutSegments("2:00"));
  Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.save(true);Settings.PLAYBACK_SPEED_DEFAULT.save(1.25f);RememberPlaybackSpeedPatch.newVideoStarted(null);line("speed.override",RememberPlaybackSpeedPatch.getPlaybackSpeedOverride());line("speed.override-consumed",RememberPlaybackSpeedPatch.getPlaybackSpeedOverride());
  RememberPlaybackSpeedPatch.userSelectedPlaybackSpeed(2f);ops.advance(749);line("speed.before-debounce",Settings.PLAYBACK_SPEED_DEFAULT.get());ops.advance(1);line("speed.after-debounce",Settings.PLAYBACK_SPEED_DEFAULT.get());
  line("settings.reboot",Settings.SPEED_TAP_AND_HOLD.rebootApp);
  line("effects.http-total",ops.requests);check(!trace.toString().contains("|null\n"),"No missing trace values");
  return trace.toString();
 }
 public static void main(String[] ignored)throws Exception {System.out.print(run());}
}
