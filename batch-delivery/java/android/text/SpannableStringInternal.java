package android.text;
import java.util.*;
public class SpannableStringInternal implements Spannable {protected String text;public static final class Span {public Object value;public int start,end,flags;Span(Object v,int s,int e,int f){value=v;start=s;end=e;flags=f;}}
public final List<Span> spans=new ArrayList<>();protected SpannableStringInternal(CharSequence s){text=s.toString();if(s instanceof Spanned p)for(Object x:p.getSpans(0,s.length(),Object.class))setSpan(x,p.getSpanStart(x),p.getSpanEnd(x),p.getSpanFlags(x));}
public int length(){return text.length();}public char charAt(int i){return text.charAt(i);}public CharSequence subSequence(int s,int e){return text.substring(s,e);}public String toString(){return text;}
public void setSpan(Object v,int s,int e,int f){if(s<0||e<s||e>length())throw new IndexOutOfBoundsException();removeSpan(v);spans.add(new Span(v,s,e,f));}
public void removeSpan(Object v){spans.removeIf(x->x.value==v);}public int getSpanFlags(Object v){for(Span x:spans)if(x.value==v)return x.flags;return 0;}
public int getSpanStart(Object v){for(Span x:spans)if(x.value==v)return x.start;return -1;}public int getSpanEnd(Object v){for(Span x:spans)if(x.value==v)return x.end;return -1;}
@SuppressWarnings("unchecked") public <T> T[] getSpans(int s,int e,Class<T> c){List<T> result=new ArrayList<>();for(Span x:spans){if(x.start>e||x.end<s)continue;if(x.start!=x.end&&s!=e&&(x.start==e||x.end==s))continue;if(c.isInstance(x.value))result.add(c.cast(x.value));}return result.toArray((T[])java.lang.reflect.Array.newInstance(c,result.size()));}}
