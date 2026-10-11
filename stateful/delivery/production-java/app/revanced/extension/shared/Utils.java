package app.revanced.extension.shared;
import rv.platform.NativeBridge;
public final class Utils {
    public static void runOnMainThreadDelayed(Runnable runnable,long delay){
        NativeBridge.postMain(() -> {try {runnable.run();}catch(Exception ex){Logger.printException(() -> "Main callback failure",ex);}},delay);
    }
    public static void showToastShort(String text){NativeBridge.showToast(text);}
}
