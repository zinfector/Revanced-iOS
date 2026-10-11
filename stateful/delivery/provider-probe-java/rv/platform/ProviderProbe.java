package rv.platform;
public final class ProviderProbe {
    private static volatile boolean called;
    public static void enqueueZero(){NativeBridge.postMain(() -> called=true,0);}
    public static boolean called(){return called;}
    public static int throwingCalls;
    public static int throwingCalls(){return throwingCalls;}
    public static void enqueueThrowing(){
        app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
            throwingCalls=throwingCalls+1;throw new IllegalStateException("Injected callback failure");
        },0);
    }
}
