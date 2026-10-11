package app.revanced.extension.shared;
public final class StringRef {private final String key;public static final StringRef empty=new StringRef("");private StringRef(String key){this.key=key;}
 public static StringRef sf(String key){return new StringRef(key);}public static String str(String key,Object... args){return rv.batch.Platform.resource(key,args);}
 public String toString(){return str(key);}}
