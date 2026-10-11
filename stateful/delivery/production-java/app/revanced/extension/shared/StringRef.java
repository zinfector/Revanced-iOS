package app.revanced.extension.shared;
public final class StringRef {public static String str(String key,Object... args){
if(key.equals("revanced_remember_playback_speed_toast"))return String.format(java.util.Locale.getDefault(),"Changed default speed to: %s",args);
throw new IllegalStateException("Unbound resource: "+key);}}
