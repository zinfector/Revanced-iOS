package android.text;public final class TextUtils {public static boolean isEmpty(CharSequence s){return s==null||s.length()==0;}
public static String join(CharSequence sep,Iterable<?> values){StringBuilder b=new StringBuilder();boolean first=true;for(Object x:values){if(!first)b.append(sep);first=false;b.append(x);}return b.toString();}}
