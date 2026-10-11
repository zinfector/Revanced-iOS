package app.revanced.extension.youtube.shared;
import java.util.*;import kotlin.Unit;import kotlin.jvm.functions.Function1;
public final class Event<T>{private final Set<Function1<T,Unit>> observers=Collections.synchronizedSet(new LinkedHashSet<>());
 public void addObserver(Function1<T,Unit> observer){observers.add(observer);}public void removeObserver(Function1<T,Unit> observer){observers.remove(observer);}
 public void invoke(T value){for(Function1<T,Unit> observer:observers)observer.invoke(value);}}
