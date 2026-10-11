package rv.batch;
import java.net.*;import java.io.*;import java.util.*;
public final class Platform {
 public interface Operations {Object read(String key,Object fallback);void write(String key,Object value);long wallMillis();void post(Runnable callback,long delay,boolean main);String resource(String key,Object[] args);Object invoke(String operation,Object[] args);}
 public static Operations operations;private static Operations ops(){if(operations==null)throw new IllegalStateException("Operations not configured");return operations;}
 public static Object read(String key,Object fallback){return ops().read(key,fallback);}public static void write(String key,Object value){ops().write(key,value);}
 public static long wallMillis(){return ops().wallMillis();}public static void post(Runnable callback,long delay,boolean main){ops().post(callback,delay,main);}
 public static String resource(String key,Object[] args){return ops().resource(key,args);}public static Object invoke(String op,Object... args){return ops().invoke(op,args);}public static void present(String op,Object... args){invoke(op,args);}
 public static boolean debug(){return (Boolean)read("debug",false);}public static void log(String message){invoke("log",message);}
}
