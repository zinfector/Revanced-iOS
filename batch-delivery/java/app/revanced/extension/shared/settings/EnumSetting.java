package app.revanced.extension.shared.settings;
public final class EnumSetting<T extends Enum<T>> extends Setting {
 public final T defaultValue;public EnumSetting(String key,T value,Object... flags){super(key,flags);defaultValue=value;}
 public T get(){String name=(String)rv.batch.Platform.read(key,defaultValue.name());return Enum.valueOf(defaultValue.getDeclaringClass(),name);}
 public void save(T value){rv.batch.Platform.write(key,value.name());}public T resetToDefault(){save(defaultValue);return defaultValue;}}
