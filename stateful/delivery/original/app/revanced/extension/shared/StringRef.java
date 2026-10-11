package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class StringRef {
    public static String str(String key,Object... args){Simulator.event("localize|"+key);return key+"|"+args[0];}
}
