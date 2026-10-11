package rv.platform;
public final class NativeBridge {
    public static native boolean readBoolean(String key, boolean fallback);
    public static native float readFloat(String key, float fallback);
    public static native void writeFloat(String key, float value);
    public static native long wallMillis();
    public static native void postMain(Runnable callback, long milliseconds);
    public static native void showToast(String message);
    public static native boolean debugEnabled();
    public static native void log(String message);
}
