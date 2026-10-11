package rv.batch;
public final class NativeOperations implements Platform.Operations {
 public native Object read(String key,Object fallback);
 public native void write(String key,Object value);
 public native long wallMillis();
 public native void post(Runnable callback,long delay,boolean main);
 public native String resource(String key,Object[] args);
 public native Object invoke(String operation,Object[] args);
 public static void install(){Platform.operations=new NativeOperations();}
}
