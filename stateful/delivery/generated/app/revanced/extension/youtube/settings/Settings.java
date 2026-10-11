package app.revanced.extension.youtube.settings;
import rv.translated.Simulator;
public final class Settings {
    public static final class BooleanSetting {
        public final String key; public boolean value;
        public BooleanSetting(String key,boolean initial){this.key=key;value=initial;}
        public boolean get(){Simulator.event("read|"+key);return value;}
    }
    public static final class FloatSetting {
        public final String key; public float value;
        public FloatSetting(String key,float initial){this.key=key;value=initial;}
        public float get(){Simulator.event("read|"+key);return value;}
        public void save(float number){Simulator.event("save|"+key+"|"+Float.floatToRawIntBits(number));value=number;}
    }
    public static final FloatSetting PLAYBACK_SPEED_DEFAULT=new FloatSetting("revanced_playback_speed_default",-2.0f);
    public static final BooleanSetting REMEMBER_PLAYBACK_SPEED_LAST_SELECTED=new BooleanSetting("revanced_remember_playback_speed_last_selected",false);
    public static final BooleanSetting REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST=new BooleanSetting("revanced_remember_playback_speed_last_selected_toast",true);
}
