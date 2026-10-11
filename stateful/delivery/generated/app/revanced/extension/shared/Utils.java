package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class Utils {
    public static void runOnMainThreadDelayed(Runnable runnable,long delay){
        Simulator.post(() -> {try {runnable.run();}catch(Exception ex){Simulator.trace.add("callback_error|"+ex.getClass().getSimpleName());}},delay);
    }
    public static void showToastShort(String text){Simulator.event("toast|"+text);}
}
