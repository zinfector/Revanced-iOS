"""Reusable language/platform seams. No ad, dislike-cache, or skip algorithm."""
import json
import re
from pathlib import Path

SP='app/revanced/extension/shared/'
YP='app/revanced/extension/youtube/'


def write(p,text):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf-8',newline='\n')
def generate_kit(kit,original,facts,out):
    source='\n'.join(p.read_text(encoding='utf-8') for p in original.rglob('*.java'))
    selected=[f for f in facts if re.search(r'\b'+re.escape(f['name'])+r'\b',source)]
    enumowners={'SponsorBlockDuration':'app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.SponsorBlockDuration',
                'CategoryBehaviour':'app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour'}
    def value(node):
        k=node['kind']
        if k=='literal':return json.dumps(node['value'],ensure_ascii=True) if isinstance(node['value'],str) else str(node['value']).lower()
        if k=='reference':
            name=node['name']
            if name in ('TRUE','FALSE'):return name.lower()
            if name in ('SKIP_AUTOMATICALLY','SKIP_AUTOMATICALLY_ONCE','MANUAL_SKIP','IGNORE'):return enumowners['CategoryBehaviour']+'.'+name
            return enumowners.get(name,name)
        if k=='member':return value(node['owner'])+'.'+node['name']
        if k=='UNARY_MINUS':return '-'+value(node['value'])
        raise ValueError('Unresolved setting default '+str(node))
    declarations=[]
    for f in selected:
        typ=f['type']
        for name,owner in enumowners.items():typ=typ.replace('<'+name+'>','<'+owner+'>')
        default=value(f['arguments'][1]);base=typ.split('<')[0]
        if base=='FloatSetting':default+='f'
        if base=='LongSetting':default+='L'
        reboot=f['arguments'][2] if len(f['arguments'])>2 else None
        flag=','+value(reboot) if reboot and reboot['kind']=='literal' and type(reboot['value']) is bool else ''
        declarations.append('public static final '+typ+' '+f['name']+'=new '+typ+'('+value(f['arguments'][0])+','+default+flag+');')
    write(kit/(YP+'settings/Settings.java'),'package app.revanced.extension.youtube.settings;\nimport app.revanced.extension.shared.settings.*;\npublic final class Settings {\n'+'\n'.join(declarations)+'\n}\n')
    write(out/'setting-facts.json',json.dumps(selected,indent=2)+'\n')
    for typ,jtype,box in [('Boolean','boolean','Boolean'),('Float','float','Float'),('Long','long','Long'),('Integer','int','Integer'),('String','String','String')]:
        write(kit/(SP+'settings/'+typ+'Setting.java'),'package app.revanced.extension.shared.settings;\n'+
              'public class '+typ+'Setting extends Setting {public final '+box+' defaultValue;'+
              'public '+typ+'Setting(String key,'+box+' value,Object... flags){super(key,flags);defaultValue=value;}'+
              'public '+box+' get(){return ('+box+')rv.batch.Platform.read(key,defaultValue);}'+
              'public void save('+box+' value){rv.batch.Platform.write(key,value);}'+
              'public boolean isSetToDefault(){return java.util.Objects.equals(get(),defaultValue);}'+
              'public '+box+' resetToDefault(){save(defaultValue);return defaultValue;}}\n')
    write(kit/(SP+'settings/EnumSetting.java'),'''package app.revanced.extension.shared.settings;
public final class EnumSetting<T extends Enum<T>> extends Setting {
 public final T defaultValue;public EnumSetting(String key,T value,Object... flags){super(key,flags);defaultValue=value;}
 public T get(){String name=(String)rv.batch.Platform.read(key,defaultValue.name());return Enum.valueOf(defaultValue.getDeclaringClass(),name);}
 public void save(T value){rv.batch.Platform.write(key,value.name());}public T resetToDefault(){save(defaultValue);return defaultValue;}}
''')
    write(kit/(SP+'settings/Setting.java'),'''package app.revanced.extension.shared.settings;
import android.content.Context;
public abstract class Setting {public final String key;public final boolean rebootApp;public Setting(String key,Object... flags){this.key=key;rebootApp=flags.length>0&&flags[0] instanceof Boolean&&(Boolean)flags[0];}
 public boolean isAvailable(){return true;}public boolean isSetToDefault(){return false;}
 public interface ImportExportCallback {void settingsImported(Context context);void settingsExported(Context context);}}
''')
    # Kotlin constants/pure predicate expressions are derived from the original enum sources.
    ytsrc=original.parent.parent.parent/'revanced-patches-main/extensions/youtube/src/main/java'
    # The authoritative workspace source is outside fresh generation directories.
    upstream=Path(__file__).resolve().parent.parent.parent/'revanced-patches-main/extensions/youtube/src/main/java'
    for name in ('PlayerType','PlayerControlsVisibility','VideoState'):
        path=upstream/(YP+'shared/'+name+'.kt');text=path.read_text(encoding='utf-8')
        prefix=text.split('companion object')[0]
        constants=re.findall(r'^\s{4}([A-Z][A-Z_]+)\s*[,;]',prefix,re.M)
        if not constants:raise ValueError('Cannot derive enum '+name)
        methods=[]
        for m in re.finditer(r'fun (is\w+)\(\): Boolean\s*\{\s*return ([^\n]+)\s*\}',text):
            expression=m[2]
            if not re.fullmatch(r'[\w .()|&=!]+',expression):raise ValueError('Unsupported Kotlin predicate')
            methods.append('public boolean '+m[1]+'(){return '+expression+';}')
        write(kit/(YP+'shared/'+name+'.java'),'package app.revanced.extension.youtube.shared;\npublic enum '+name+'{'+','.join(constants)+';'+
              'private static volatile '+name+' current='+constants[0]+';private static final Event<'+name+'> event=new Event<>();'+
              'public static '+name+' getCurrent(){return current;}public static void setFromString(String value){'+name+' next=valueOf(value);if(next!=current){current=next;event.invoke(next);}}'+
              'public static Event<'+name+'> getOnChange(){return event;}'+''.join(methods)+'}\n')
    write(kit/'kotlin/Unit.java','package kotlin; public final class Unit {public static final Unit INSTANCE=new Unit();private Unit(){}}\n')
    write(kit/'kotlin/jvm/functions/Function1.java','package kotlin.jvm.functions; public interface Function1<T,R>{R invoke(T value);}\n')
    write(kit/(YP+'shared/Event.java'),'''package app.revanced.extension.youtube.shared;
import java.util.*;import kotlin.Unit;import kotlin.jvm.functions.Function1;
public final class Event<T>{private final Set<Function1<T,Unit>> observers=Collections.synchronizedSet(new LinkedHashSet<>());
 public void addObserver(Function1<T,Unit> observer){observers.add(observer);}public void removeObserver(Function1<T,Unit> observer){observers.remove(observer);}
 public void invoke(T value){for(Function1<T,Unit> observer:observers)observer.invoke(value);}}
''')
    write(kit/(YP+'shared/ShortsPlayerState.java'),'package app.revanced.extension.youtube.shared;public final class ShortsPlayerState {private static volatile boolean open;private static final Event<Boolean> event=new Event<>();public static Event<Boolean> getOnChange(){return event;}public static boolean isOpen(){return open;}public static void setOpen(boolean value){if(open!=value){open=value;event.invoke(value);}}}\n')
    write(kit/(SP+'Logger.java'),'''package app.revanced.extension.shared;
public final class Logger {public interface LogMessage{String buildMessageString();}
 public static void printDebug(LogMessage message){if(rv.batch.Platform.debug())rv.batch.Platform.log(message.buildMessageString());}
 public static void printDebug(LogMessage message,Exception error){if(rv.batch.Platform.debug())rv.batch.Platform.log(message.buildMessageString()+" | "+error);}
 public static void printInfo(LogMessage message){rv.batch.Platform.log(message.buildMessageString());}
 public static void printInfo(LogMessage message,Exception error){rv.batch.Platform.log(message.buildMessageString()+" | "+error);}
 public static void printException(LogMessage message){rv.batch.Platform.log(message.buildMessageString());}
 public static void printException(LogMessage message,Exception error){rv.batch.Platform.log(message.buildMessageString()+" | "+error);}}
''')
    write(kit/(SP+'StringRef.java'),'''package app.revanced.extension.shared;
public final class StringRef {private final String key;public static final StringRef empty=new StringRef("");private StringRef(String key){this.key=key;}
 public static StringRef sf(String key){return new StringRef(key);}public static String str(String key,Object... args){return rv.batch.Platform.resource(key,args);}
 public String toString(){return str(key);}}
''')
    for name in ('NonNull','Nullable','ColorInt','GuardedBy','RequiresApi','SuppressLint'):
        package='android.annotation' if name=='SuppressLint' else 'androidx.annotation'
        write(kit/(package.replace('.','/')+'/'+name+'.java'),'package '+package+';public @interface '+name+'{String[] value() default {}; }\n')
    # Start with SDK signatures for generic operations; the executable facade is compiled separately.
    for name,text in SOURCES.items():write(kit/name,text)


SOURCES={
 'rv/batch/Platform.java':'''package rv.batch;
import java.net.*;import java.io.*;import java.util.*;
public final class Platform {
 public interface Operations {Object read(String key,Object fallback);void write(String key,Object value);long wallMillis();void post(Runnable callback,long delay,boolean main);String resource(String key,Object[] args);Object invoke(String operation,Object[] args);}
 public static Operations operations;private static Operations ops(){if(operations==null)throw new IllegalStateException("Operations not configured");return operations;}
 public static Object read(String key,Object fallback){return ops().read(key,fallback);}public static void write(String key,Object value){ops().write(key,value);}
 public static long wallMillis(){return ops().wallMillis();}public static void post(Runnable callback,long delay,boolean main){ops().post(callback,delay,main);}
 public static String resource(String key,Object[] args){return ops().resource(key,args);}public static Object invoke(String op,Object... args){return ops().invoke(op,args);}public static void present(String op,Object... args){invoke(op,args);}
 public static boolean debug(){return (Boolean)read("debug",false);}public static void log(String message){invoke("log",message);}
}
''',
 SP+'Utils.java':'''package app.revanced.extension.shared;
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
 public static String getTextDirectionString(){return isRightToLeftLocale()?"\\u200f":"\\u200e";}
 public static int adjustColorBrightness(int c,float light,float dark){float f=isDarkModeEnabled()?dark:light;int r=Color.red(c),g=Color.green(c),b=Color.blue(c);float t=1-1/f;
 if(f>1){r=Math.round(r+(255-r)*t);g=Math.round(g+(255-g)*t);b=Math.round(b+(255-b)*t);}else{r=Math.round(r*f);g=Math.round(g*f);b=Math.round(b*f);}return Color.argb(Color.alpha(c),Math.min(255,Math.max(0,r)),Math.min(255,Math.max(0,g)),Math.min(255,Math.max(0,b)));}
 public static void hideViewByRemovingFromParentUnderCondition(boolean hide,View v){if(hide&&v.getParent() instanceof ViewGroup)((ViewGroup)v.getParent()).removeView(v);}
}
''',
 SP+'ResourceType.java':'package app.revanced.extension.shared;public enum ResourceType{ID,LAYOUT,DRAWABLE,STRING,COLOR,INTEGER,ANIM}\n',
 SP+'ui/Dim.java':'''package app.revanced.extension.shared.ui;
public final class Dim {public static int dp(float n){return Math.round(n);}public static float[] roundedCorners(int n){return new float[]{n,n,n,n,n,n,n,n};}
 public static final int dp1=1,dp2=2,dp4=4,dp7=7,dp8=8,dp10=10,dp12=12,dp14=14,dp16=16,dp18=18,dp20=20,dp24=24,dp28=28,dp32=32,dp36=36,dp40=40,dp48=48;}
''',
 YP+'patches/VideoInformation.java':'''package app.revanced.extension.youtube.patches;
public final class VideoInformation {public interface PlaybackController{}
 public static long getVideoTime(){return ((Number)rv.batch.Platform.read("player.position_ms",0L)).longValue();}
 public static long getVideoLength(){return ((Number)rv.batch.Platform.read("player.duration_ms",0L)).longValue();}
 public static float getPlaybackSpeed(){return ((Number)rv.batch.Platform.read("player.rate",1f)).floatValue();}
 public static boolean seekTo(long value){return (Boolean)rv.batch.Platform.invoke("player.seek_ms",value);}
 public static boolean lastPlayerResponseIsShort(){return (Boolean)rv.batch.Platform.read("player.is_short",false);}
 public static String getVideoId(){return (String)rv.batch.Platform.read("player.video_id","");}
 public static void overridePlaybackSpeed(float rate){rv.batch.Platform.present("player.set_rate",rate);}}
''',
 YP+'videoplayer/PlayerControlButton.java':'''package app.revanced.extension.youtube.videoplayer;
public final class PlayerControlButton {public static final int fadeInDuration=200;public static int getDialogBackgroundColor(){return 0xffffffff;}}
''',
 YP+'patches/litho/PlaybackSpeedMenuFilter.java':'package app.revanced.extension.youtube.patches.litho;public final class PlaybackSpeedMenuFilter {public static volatile boolean isPlaybackRateSelectorMenuVisible,isOldPlaybackSpeedMenuVisible;}\n',
 YP+'sponsorblock/ui/SponsorBlockViewController.java':'''package app.revanced.extension.youtube.sponsorblock.ui;
import android.content.Context;import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment;
public final class SponsorBlockViewController {
 public static void hideAll(){rv.batch.Platform.present("roles.hide_all","sponsorblock");}
 public static void hideNewSegmentLayout(){rv.batch.Platform.present("role.hide","new-segment");}
 public static void showSkipHighlightButton(SponsorSegment segment){rv.batch.Platform.present("role.button.show","sponsor-highlight",segment);}
 public static void hideSkipHighlightButton(){rv.batch.Platform.present("role.button.hide","sponsor-highlight");}
 public static void showSkipSegmentButton(SponsorSegment segment){rv.batch.Platform.present("role.button.show","sponsor-skip",segment);}
 public static void hideSkipSegmentButton(){rv.batch.Platform.present("role.button.hide","sponsor-skip");}
 public static void setSkipSegment(SponsorSegment segment){rv.batch.Platform.present("role.button.value","sponsor-skip",segment);}
 public static Context getOverLaysViewGroupContext(){return (Context)rv.batch.Platform.invoke("context.player_overlay");}}
''',
 YP+'shared/NavigationBar.java':'''package app.revanced.extension.youtube.shared;
public final class NavigationBar {public enum NavigationButton {HOME,SHORTS,EXPLORE,CREATE,SUBSCRIPTIONS,LIBRARY,YOU;
 public static NavigationButton getSelected(){return getSelectedNavigationButton();}public static NavigationButton getSelectedNavigationButton(){return NavigationButton.valueOf((String)rv.batch.Platform.read("navigation.selected","HOME"));}}public static boolean isSearchBarActive(){return (Boolean)rv.batch.Platform.read("navigation.search_active",false);}}
''',
 YP+'shared/EngagementPanel.java':'package app.revanced.extension.youtube.shared;public final class EngagementPanel{public static boolean isDescription(){return (Boolean)rv.batch.Platform.read("engagement.description",false);}}\n',
 SP+'patches/litho/LithoFilterPatch.java':'package app.revanced.extension.shared.patches.litho;public final class LithoFilterPatch{}\n',
 YP+'sponsorblock/ui/SponsorBlockPreferenceGroup.java':'package app.revanced.extension.youtube.sponsorblock.ui;public final class SponsorBlockPreferenceGroup {public static boolean settingsImported;}\n',
 SP+'ui/SheetBottomDialog.java':'''package app.revanced.extension.shared.ui;
import android.content.*;import android.app.*;import android.view.*;import android.widget.*;
public final class SheetBottomDialog {
 public static class DraggableLinearLayout extends LinearLayout{public DraggableLinearLayout(Context c){super(c);}}
 public static class SlideDialog extends Dialog{public SlideDialog(Context c){super(c);}}
 public static DraggableLinearLayout createMainLayout(Context c,int color){DraggableLinearLayout v=new DraggableLinearLayout(c);v.setOrientation(LinearLayout.VERTICAL);v.setBackgroundColor(color);return v;}
 public static SlideDialog createSlideDialog(Context c,View v,int duration){SlideDialog d=new SlideDialog(c);d.setContentView(v);return d;}}
''',
 SP+'ui/CustomDialog.java':'''package app.revanced.extension.shared.ui;
import android.app.*;import android.content.*;import android.widget.*;import android.util.Pair;
public final class CustomDialog {public static Pair<Dialog,LinearLayout> create(Context c,CharSequence title,CharSequence message,EditText edit,String ok,Runnable yes,Runnable no,String neutral,Runnable extra,boolean dismiss){
 LinearLayout layout=new LinearLayout(c);Dialog dialog=new Dialog(c);dialog.setContentView(layout);rv.batch.Platform.present("dialog.configure",dialog,title,message,edit,ok,yes,no,neutral,extra,dismiss);return Pair.create(dialog,layout);}}
''',
 'com/google/android/libraries/youtube/rendering/ui/pivotbar/PivotBar.java':'package com.google.android.libraries.youtube.rendering.ui.pivotbar;public class PivotBar extends android.view.View {public PivotBar(android.content.Context c){super(c);}}\n',
 'android/support/v7/widget/RecyclerView.java':'package android.support.v7.widget;public class RecyclerView extends android.view.ViewGroup {public RecyclerView(android.content.Context c){super(c);}protected void onLayout(boolean changed,int l,int t,int r,int b){}}\n',
}
