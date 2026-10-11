package app.revanced.extension.shared;
import android.content.*;import android.graphics.*;import android.view.*;import android.view.animation.*;import android.widget.*;
import java.util.*;import java.util.concurrent.*;import java.util.function.*;
public final class Utils {
 public static void verifyOnMainThread(){if(!(Boolean)rv.batch.Platform.invoke("thread.is_main"))throw new IllegalStateException("Main thread required");}public static void verifyOffMainThread(){if((Boolean)rv.batch.Platform.invoke("thread.is_main"))throw new IllegalStateException("Background thread required");}
 public static void runOnMainThread(Runnable r){runOnMainThreadDelayed(r,0);}public static void runOnMainThreadNowOrLater(Runnable r){if((Boolean)rv.batch.Platform.invoke("thread.is_main"))r.run();else runOnMainThread(r);}
 public static void runOnMainThreadDelayed(Runnable r,long delay){rv.batch.Platform.post(()->{try{r.run();}catch(Exception e){Logger.printException(()->"Main callback failure",e);}},delay,true);}
 public static void runOnBackgroundThread(Runnable r){rv.batch.Platform.post(r,0,false);}public static <T> Future<T> submitOnBackgroundThread(Callable<T> c){FutureTask<T> task=new FutureTask<>(c);runOnBackgroundThread(task);return task;}
 public static void showToastShort(String s){showToast(s,0);}public static void showToastLong(String s){showToast(s,1);}public static void showToast(String s,int duration){rv.batch.Platform.present("toast",s,duration);}
 public static boolean isNetworkConnected(){return (Boolean)rv.batch.Platform.invoke("network.connected");}public static Locale getAppLocale(){return Locale.forLanguageTag((String)rv.batch.Platform.invoke("locale"));}public static boolean isDarkThemeEnabled(){return (Boolean)rv.batch.Platform.invoke("theme.dark");}
 public static boolean containsNumber(CharSequence s){for(int i=0;i<s.length();i++)if(Character.isDigit(s.charAt(i)))return true;return false;}
 public static boolean containsAny(String s,String... parts){for(String p:parts)if(s.contains(p))return true;return false;}
 public static int getAppForegroundColor(){return ((Number)rv.batch.Platform.invoke("theme.foreground")).intValue();}public static int getAppBackgroundColor(){return ((Number)rv.batch.Platform.invoke("theme.background")).intValue();}public static int getDialogBackgroundColor(){return getAppBackgroundColor();}
 public static int getResourceIdentifierOrThrow(ResourceType type,String key){return ((Number)rv.batch.Platform.invoke("resource.id",type.name(),key)).intValue();}public static int getResourceInteger(String key){return ((Number)rv.batch.Platform.invoke("resource.integer",key)).intValue();}
 public static Animation getResourceAnimation(String key){return new android.view.animation.AlphaAnimation(0,1);}
 public static void setDialogWindowParameters(Window w,int gravity,int y,int margin,boolean dim){rv.batch.Platform.present("window.layout",w,gravity,y,margin,dim);}
 public static Context getContext(){return (Context)rv.batch.Platform.invoke("context.application");}public static String getAppVersionName(){return (String)rv.batch.Platform.invoke("application.version");}public static String getPatchesReleaseVersion(){return "stateful-batch";}
 public static void doNothingForDuration(long millis){try{Thread.sleep(millis);}catch(InterruptedException e){Thread.currentThread().interrupt();}}
 public static ViewParent getParentView(View v,int count){ViewParent p=v.getParent();int depth=0;while(++depth<count && p!=null)p=p.getParent();return depth==count?p:null;}
 public static void hideViewUnderCondition(app.revanced.extension.shared.settings.BooleanSetting s,View v){if(s.get())v.setVisibility(View.GONE);}
 public static void hideViewBy0dpUnderCondition(app.revanced.extension.shared.settings.BooleanSetting s,View v){if(s.get()){ViewGroup.LayoutParams p=v.getLayoutParams();p.width=0;p.height=0;v.setLayoutParams(p);}}
 public static <T,V> Map<T,V> createSizeRestrictedMap(int max){return new LinkedHashMap<T,V>(2*max){protected boolean removeEldestEntry(Map.Entry<T,V> e){return size()>max;}};}
 public static float clamp(float value,float low,float high){return Math.max(low,Math.min(value,high));}
 public static boolean isDarkModeEnabled(){return isDarkThemeEnabled();}public static boolean isRightToLeftLocale(){String s=getAppLocale().getLanguage();return s.equals("ar")||s.equals("he")||s.equals("fa")||s.equals("ur");}
 public static String getTextDirectionString(){return isRightToLeftLocale()?"\u200f":"\u200e";}
 public static int adjustColorBrightness(int c,float light,float dark){float f=isDarkModeEnabled()?dark:light;int r=Color.red(c),g=Color.green(c),b=Color.blue(c);float t=1-1/f;
 if(f>1){r=Math.round(r+(255-r)*t);g=Math.round(g+(255-g)*t);b=Math.round(b+(255-b)*t);}else{r=Math.round(r*f);g=Math.round(g*f);b=Math.round(b*f);}return Color.argb(Color.alpha(c),Math.min(255,Math.max(0,r)),Math.min(255,Math.max(0,g)),Math.min(255,Math.max(0,b)));}
 public static void hideViewByRemovingFromParentUnderCondition(boolean hide,View v){if(hide&&v.getParent() instanceof ViewGroup)((ViewGroup)v.getParent()).removeView(v);}
}
