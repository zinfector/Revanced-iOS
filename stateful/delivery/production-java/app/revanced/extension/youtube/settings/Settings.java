package app.revanced.extension.youtube.settings;
import rv.platform.NativeBridge;
public final class Settings {
    public static final class BooleanSetting {
        private final String key; private final boolean fallback;
        public BooleanSetting(String key,boolean fallback){this.key=key;this.fallback=fallback;}
        public boolean get(){return NativeBridge.readBoolean(key,fallback);}
    }
    public static final class FloatSetting {
        private final String key; private final float fallback;
        public FloatSetting(String key,float fallback){this.key=key;this.fallback=fallback;}
        public float get(){return NativeBridge.readFloat(key,fallback);}
        public void save(float value){NativeBridge.writeFloat(key,value);}
    }
public static final FloatSetting PLAYBACK_SPEED_DEFAULT=new FloatSetting("revanced_playback_speed_default",-2.0f);
public static final BooleanSetting REMEMBER_PLAYBACK_SPEED_LAST_SELECTED=new BooleanSetting("revanced_remember_playback_speed_last_selected",false);
public static final BooleanSetting REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST=new BooleanSetting("revanced_remember_playback_speed_last_selected_toast",true);
}
