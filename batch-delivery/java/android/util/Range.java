package android.util;
public final class Range<T extends Comparable<? super T>>{private final T lower,upper;
public Range(T l,T u){if(l==null||u==null||l.compareTo(u)>0)throw new IllegalArgumentException();lower=l;upper=u;}
public static <T extends Comparable<? super T>> Range<T> create(T l,T u){return new Range<>(l,u);}
public T getLower(){return lower;}public T getUpper(){return upper;}public boolean contains(T v){return lower.compareTo(v)<=0&&upper.compareTo(v)>=0;}
public Range<T> extend(Range<T> v){return create(lower.compareTo(v.lower)<0?lower:v.lower,upper.compareTo(v.upper)>0?upper:v.upper);}
public boolean equals(Object o){if(!(o instanceof Range<?>))return false;Range<?> r=(Range<?>)o;return lower.equals(r.lower)&&upper.equals(r.upper);}public int hashCode(){return lower.hashCode()^upper.hashCode();}}
