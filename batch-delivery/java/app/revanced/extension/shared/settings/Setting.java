package app.revanced.extension.shared.settings;
import android.content.Context;
public abstract class Setting {public final String key;public final boolean rebootApp;public Setting(String key,Object... flags){this.key=key;rebootApp=flags.length>0&&flags[0] instanceof Boolean&&(Boolean)flags[0];}
 public boolean isAvailable(){return true;}public boolean isSetToDefault(){return false;}
 public interface ImportExportCallback {void settingsImported(Context context);void settingsExported(Context context);}}
