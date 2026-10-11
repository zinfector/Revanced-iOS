package app.revanced.extension.youtube.patches;
public final class VideoInformation {public interface PlaybackController{}
 public static long getVideoTime(){return ((Number)rv.batch.Platform.read("player.position_ms",0L)).longValue();}
 public static long getVideoLength(){return ((Number)rv.batch.Platform.read("player.duration_ms",0L)).longValue();}
 public static float getPlaybackSpeed(){return ((Number)rv.batch.Platform.read("player.rate",1f)).floatValue();}
 public static boolean seekTo(long value){return (Boolean)rv.batch.Platform.invoke("player.seek_ms",value);}
 public static boolean lastPlayerResponseIsShort(){return (Boolean)rv.batch.Platform.read("player.is_short",false);}
 public static String getVideoId(){return (String)rv.batch.Platform.read("player.video_id","");}
 public static void overridePlaybackSpeed(float rate){rv.batch.Platform.present("player.set_rate",rate);}}
