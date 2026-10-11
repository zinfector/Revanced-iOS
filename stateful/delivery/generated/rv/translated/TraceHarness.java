package rv.translated;

import java.lang.reflect.Field;
import app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch;
import app.revanced.extension.youtube.settings.Settings;

public final class TraceHarness {
    private static void field(String name,Object value) throws Exception {
        Field f=RememberPlaybackSpeedPatch.class.getDeclaredField(name); f.setAccessible(true); f.set(null,value);
    }
    private static String state() throws Exception {
        Field b=RememberPlaybackSpeedPatch.class.getDeclaredField("newVideoStarted"), l=RememberPlaybackSpeedPatch.class.getDeclaredField("lastTimeSpeedChanged");
        b.setAccessible(true);l.setAccessible(true);
        return "state|"+b.getBoolean(null)+"|"+l.getLong(null)+"|"+Float.floatToRawIntBits(Settings.PLAYBACK_SPEED_DEFAULT.value)+"|"+Simulator.pending();
    }
    private static void reset() throws Exception {
        Simulator.reset();field("newVideoStarted",false);field("lastTimeSpeedChanged",0L);
        Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.value=true;
        Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST.value=true;
        Settings.PLAYBACK_SPEED_DEFAULT.value=-2.0f;
    }
    private static void snapshot() throws Exception { Simulator.trace.add(state()); }
    private static void select(float rate) { RememberPlaybackSpeedPatch.userSelectedPlaybackSpeed(rate); }
    private static void override() { Simulator.trace.add("result|"+Float.floatToRawIntBits(RememberPlaybackSpeedPatch.getPlaybackSpeedOverride())); }
    public static String runCase(int test) throws Exception {
        reset();
        switch(test) {
            case 0: Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.value=false;select(2);break;
            case 1: select(1.5f);snapshot();Simulator.runThrough(1749);snapshot();Simulator.runThrough(1750);break;
            case 2: select(1.5f);Simulator.now=1200;select(2);Simulator.runThrough(1950);break;
            case 3: select(1.5f);select(2);Simulator.runThrough(1750);break;
            case 4: Settings.PLAYBACK_SPEED_DEFAULT.value=1;select(2);Simulator.now=1200;select(1);Simulator.runThrough(1950);break;
            case 5: select(2);RememberPlaybackSpeedPatch.newVideoStarted(null);Simulator.runThrough(1750);override();override();break;
            case 6: select(2);Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.value=false;Simulator.runThrough(1750);break;
            case 7: select(2);Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST.value=false;Simulator.runThrough(1750);break;
            case 8: select(10);Simulator.runThrough(1750);break;
            case 9: select(Float.NaN);Simulator.runThrough(1750);break;
            case 10: select(-0.0f);Simulator.runThrough(1750);RememberPlaybackSpeedPatch.newVideoStarted(null);override();break;
            case 11: Simulator.failure="read|revanced_remember_playback_speed_last_selected";select(2);break;
            case 12: Simulator.failure="save|revanced_playback_speed_default|1073741824";select(2);Simulator.runThrough(1750);break;
            case 13: Simulator.failure="enqueue|750|1000";select(2);break;
            case 14: Settings.PLAYBACK_SPEED_DEFAULT.value=2;RememberPlaybackSpeedPatch.newVideoStarted(null);snapshot();override();override();break;
            case 15: RememberPlaybackSpeedPatch.newVideoStarted(null);override();break;
            case 16: Simulator.debug=true;RememberPlaybackSpeedPatch.newVideoStarted(null);select(2);Simulator.runThrough(1750);break;
            case 17: select(2);Settings.PLAYBACK_SPEED_DEFAULT.value=2;Simulator.runThrough(1750);break;
            case 18: select(Float.POSITIVE_INFINITY);Simulator.runThrough(1750);break;
            case 19: select(Float.NEGATIVE_INFINITY);Simulator.runThrough(1750);break;
            default: throw new IllegalArgumentException("case");
        }
        snapshot();return Simulator.join();
    }
    public static String runAll() throws Exception {
        StringBuilder result=new StringBuilder();
        for(int i=0;i<20;i++) result.append(i).append("|").append(runCase(i)).append('\n');
        return result.toString();
    }
    public static void main(String[] args) throws Exception { System.out.print(runAll()); }
}
