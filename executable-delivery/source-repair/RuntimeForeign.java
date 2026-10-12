package rv.executable;

import java.util.*;
import java.lang.ref.WeakReference;
import java.lang.reflect.Array;
import rv.executable.ExecutableRuntime.*;

/** Object conversion, original callbacks and platform capability routing.
 * Calls to JRE members are generated separately from exact class-file APIs. */
public abstract class RuntimeForeign implements ForeignLifecycle {
    protected ExecutableRuntime runtime;
    protected final PlatformServices services;
    public RuntimeForeign(PlatformServices services){this.services=Objects.requireNonNull(services);}
    public void bind(ExecutableRuntime rt){if(runtime!=null)throw new IllegalStateException("Provider already bound");runtime=rt;}
    protected abstract Value jreCall(String method,Value receiver,Value[] args)throws Throwable;
    protected abstract Value jreField(String field,Value receiver)throws Throwable;
    protected abstract Class<?> hostClass(String type);
    protected abstract String[] hostParents(String type);
    protected abstract boolean hasDefaultMethods(String type);
    protected abstract boolean callStatic(String method);
    protected abstract boolean fieldStatic(String field);
    private final ThreadLocal<Map<String,String>> boundary=new ThreadLocal<>();
    private volatile Map<String,String> foreignFailure=Collections.emptyMap();
    private volatile Throwable foreignError;
    public Map<String,String> failureContext(){return new LinkedHashMap<>(foreignFailure);}
    private void arrayPhase(String phase,String source,String required,int length,int index){
        Map<String,String> row=boundary.get();if(row==null)return;
        row.put("phase",phase);row.put("sourceArray",source);row.put("requiredHostType",required);
        row.put("length",Integer.toString(length));row.put("elementIndex",Integer.toString(index));
    }
    protected Object exportArgument(Value value,String type,int index){
        Map<String,String> row=boundary.get();if(row!=null){row.put("argumentIndex",Integer.toString(index));row.put("phase","argument_export");}
        return exportObject(value.o(),type);
    }
    protected Object proxy(Instance source,String type){return new SourceProxy(this,source);}
    private static final class Host {
        final String type;Object value;
        Host(String type){this.type=type;}
    }
    private static final class Link {
        final WeakReference<Object> host;final WeakReference<Object> source;
        // A returned native array must retain its source component descriptor
        // until it is imported again. Arrays cannot carry an attached field.
        final Object arraySource;
        Link(Object host,Object source){this.host=new WeakReference<>(host);this.source=new WeakReference<>(source);arraySource=ArrayInterop.isArray(host) && source instanceof ArrayValue?source:null;}
        Object source(){return arraySource==null?source.get():arraySource;}
    }
    private final List<Link> links=new ArrayList<>();
    private final ThreadLocal<Set<Object>> importing=ThreadLocal.withInitial(()->Collections.newSetFromMap(new IdentityHashMap<>()));
    private final ThreadLocal<Set<Object>> exporting=ThreadLocal.withInitial(()->Collections.newSetFromMap(new IdentityHashMap<>()));
    private final Map<String,Preferences> preferences=new HashMap<>();
    final PackageManager packageManager=new PackageManager();final Resources resources=new Resources();
    private final Context context=new Context(resources);private final Looper main=new Looper();
    public Value context(){return Value.ref(context);}
    private synchronized Object linked(Object value,boolean fromHost){
        for(Iterator<Link> it=links.iterator();it.hasNext();) {
            Link link=it.next();Object h=link.host.get(),s=link.source();
            if(h==null || s==null){it.remove();continue;}
            if((fromHost?h:s)==value)return fromHost?s:h;
        }return null;
    }
    private synchronized void link(Object host,Object source){links.add(new Link(host,source));}
    private synchronized Object projection(Object source,boolean array){
        for(Iterator<Link> it=links.iterator();it.hasNext();){Link link=it.next();Object h=link.host.get(),s=link.source();if(h==null || s==null){it.remove();continue;}if(s==source && (array?ArrayInterop.isArray(h):h instanceof ReferenceProxy))return h;}return null;
    }
    public Value importValue(Object object){
        if(object==null)return Value.NULL;
        if(object instanceof SourceProxy)return Value.ref(((SourceProxy)object).source);
        Object original=linked(object,true);
        if(original!=null){
            if(original instanceof ArrayValue && ArrayInterop.isArray(object) && importing.get().add(object)) {
                ArrayValue array=(ArrayValue)original;
                try{for(int i=0;i<array.values.length;i++)runtime.arrayPut(Value.ref(array),i,primitiveImport(Array.get(object,i),array.type.substring(1)));}
                finally{importing.get().remove(object);}
            }
            return Value.ref(original);
        }
        if(object instanceof Class<?>)return Value.ref(runtime.classToken(descriptor((Class<?>)object)));
        if(ArrayInterop.isArray(object)) {
            String type=descriptor(object.getClass());int count=Array.getLength(object);
            Value result=runtime.newArray(type,count);link(object,result.o());
            importing.get().add(object);
            try{for(int i=0;i<count;i++)runtime.arrayPut(result,i,primitiveImport(Array.get(object,i),type.substring(1)));}
            finally{importing.get().remove(object);}
            return result;
        }
        return Value.ref(object);
    }
    private Value primitiveImport(Object value,String type){
        if(type.equals("Z"))return Value.i32((Boolean)value?1:0);
        if(type.equals("C"))return Value.i32((Character)value);
        if(type.equals("J"))return Value.i64(((Number)value).longValue());
        if(type.equals("F"))return Value.f32(((Number)value).floatValue());
        if(type.equals("D"))return Value.f64(((Number)value).doubleValue());
        if(type.length()==1)return Value.i32(((Number)value).intValue());
        return importValue(value);
    }
    private Object primitiveExport(Value value,String type){
        switch(type){case "Z":return value.z();case "B":return (byte)value.i();case "C":return (char)value.i();case "S":return (short)value.i();
            case "I":return value.i();case "J":return value.j();case "F":return value.f();case "D":return value.d();default:return exportObject(value.o(),type);}
    }
    public Object exportObject(Object object,String type){
        if(object==null)return null;
        if(object instanceof Host){Object value=((Host)object).value;if(value==null)throw new InvalidProgram("Unconstructed host object");return value;}
        if(object instanceof ClassToken){Class<?> cls=hostClass(((ClassToken)object).type);if(cls==null){
            cls=primitiveClass(((ClassToken)object).type);if(cls!=null)return cls;
            if(type.equals("Ljava/lang/Object;")){Object cached=projection(object,false);if(cached==null){cached=new ReferenceProxy(this,object);link(cached,object);}return cached;}
            throw new MissingOperation("native-class-object:"+((ClassToken)object).type);
        }return cls;}
        if(object instanceof Instance){
            if(!runtime.isAssignable(((Instance)object).type,type))throw new ClassCastException(type);
            Object cached=linked(object,false);
            if(cached!=null && (type.equals("Ljava/lang/Object;") || (hostClass(type)!=null && hostClass(type).isInstance(cached))))return cached;
            Object result=proxy((Instance)object,type);Class<?> expected=hostClass(type);
            if(expected!=null && !expected.isInstance(result))throw new MissingOperation("interface-proxy:"+type);
            link(result,object);return result;
        }
        if(object instanceof ArrayValue){
            if(!runtime.isAssignable(((ArrayValue)object).type,type))throw new ClassCastException(type);
            if(!type.startsWith("[")){
                Object cached=projection(object,false);if(cached==null){cached=new ArrayReferenceProxy(this,object);link(cached,object);}return cached;
            }
            ArrayValue array=(ArrayValue)object;Object cached=projection(object,true);
            Class<?> component=arrayComponent(array.type.substring(1));
            if(component==null && array.type.substring(1).startsWith("L"))component=Object.class;
            if(component==null)component=primitiveClass(array.type.substring(1));
            if(component==null)throw new MissingOperation("array-component:"+array.type);
            // Unknown source classes may project to Object[] only for a consumer
            // admitting Object elements; an exact unavailable host type stays held.
            String requiredComponent=type.substring(1);
            if(hostClass(array.type.substring(1))==null && array.type.substring(1).startsWith("L")
                    && !requiredComponent.equals("Ljava/lang/Object;"))
                throw new MissingOperation("exact-array-component:"+array.type+":"+type);
            arrayPhase("array_allocate",array.type,type,array.values.length,-1);
            Object result=cached==null?Array.newInstance(component,array.values.length):cached;
            arrayPhase("array_metadata",array.type,type,array.values.length,-1);
            ArrayInterop.ensure(result);
            if(cached==null)link(result,object);
            if(exporting.get().add(object)){
                try{for(int i=0;i<array.values.length;i++){
                    arrayPhase("array_element_export",array.type,type,array.values.length,i);
                    Object element=primitiveExport(array.values[i],array.type.substring(1));
                    arrayPhase("array_element_store",array.type,type,array.values.length,i);
                    Array.set(result,i,element);
                }}
                finally{exporting.get().remove(object);}
            }arrayPhase("array_ready_for_cast_or_call",array.type,type,array.values.length,-1);return result;
        }
        return object;
    }
    private Class<?> arrayComponent(String type){
        Class<?> result=hostClass(type);if(result!=null)return result;
        if(type.startsWith("[")){
            Class<?> nested=arrayComponent(type.substring(1));if(nested==null)return null;
            Object sample=Array.newInstance(nested,0);ArrayInterop.ensure(sample);return sample.getClass();
        }
        return primitiveClass(type);
    }
    private static class ReferenceProxy {
        final RuntimeForeign owner;final Object source;ReferenceProxy(RuntimeForeign owner,Object source){this.owner=owner;this.source=source;}
        public boolean equals(Object other){try{return owner.objectEquals(source,owner.importValue(other).o());}catch(Throwable error){return RuntimeForeign.<RuntimeException,Boolean>raise(error);}}
        public int hashCode(){try{return owner.objectHash(source);}catch(Throwable error){return RuntimeForeign.<RuntimeException,Integer>raise(error);}}
        public String toString(){try{return owner.objectString(source);}catch(Throwable error){return RuntimeForeign.<RuntimeException,String>raise(error);}}
    }
    private static final class ArrayReferenceProxy extends ReferenceProxy implements Cloneable,java.io.Serializable {
        ArrayReferenceProxy(RuntimeForeign owner,Object source){super(owner,source);}
    }
    private static Class<?> primitiveClass(String type){switch(type){case "Z":return boolean.class;case "B":return byte.class;case "C":return char.class;case "S":return short.class;case "I":return int.class;case "J":return long.class;case "F":return float.class;case "D":return double.class;default:return null;}}
    private static String descriptor(Class<?> cls){
        if(cls.isArray())return cls.getName().replace('.','/');
        if(cls.isPrimitive()){if(cls==boolean.class)return "Z";if(cls==byte.class)return "B";if(cls==char.class)return "C";if(cls==short.class)return "S";if(cls==int.class)return "I";if(cls==long.class)return "J";if(cls==float.class)return "F";if(cls==double.class)return "D";return "V";}
        return "L"+cls.getName().replace('.','/')+";";
    }
    protected void construct(Value receiver,Object object){
        if(!(receiver.o() instanceof Host))throw new InvalidProgram("Host constructor receiver");
        Host host=(Host)receiver.o();if(host.value!=null)throw new InvalidProgram("Constructor invoked twice");host.value=object;link(object,host);
    }
    public Object allocate(String type){
        if(type.equals("Landroid/os/Handler;"))return new Handler();
        if(type.equals("Landroid/content/res/Configuration;"))return new Configuration();
        Class<?> cls=hostClass(type);if(cls==null)throw new MissingOperation("allocate:"+type);
        if(cls.isInterface() || java.lang.reflect.Modifier.isAbstract(cls.getModifiers()))throw new InstantiationError(type);
        return new Host(type);
    }
    public void prepareSuperclass(Instance instance,String type){
        if(!type.equals("Ljava/lang/Enum;"))throw new MissingOperation("foreign-superclass-allocation:"+type);
        instance.foreignState.put(type,new EnumState());
    }
    public void initializeClass(String type)throws Throwable {
        if(type.startsWith("Landroid/") && AndroidOperations.knownType(type))return;
        Class<?> cls=hostClass(type);if(cls==null)throw new MissingOperation("initialize:"+type);
        // Only generated, pinned JRE class names reach this controlled loader.
        Class.forName(cls.getName(),true,cls.getClassLoader());
    }
    public void initializeDefaults(String type)throws Throwable {
        String[] parents=hostParents(type);if(parents==null)throw new MissingOperation("default-interface-initialization:"+type);
        for(String parent:parents)if(hostClass(parent)!=null && hostClass(parent).isInterface())initializeDefaults(parent);
        if(hasDefaultMethods(type))initializeClass(type);
    }
    public boolean isAssignable(String source,String target){return ancestry(source,target,new HashSet<>());}
    private boolean ancestry(String source,String target,Set<String> seen){
        if(source.equals(target))return true;if(!seen.add(source))return false;
        if(target.equals("Ljava/lang/Object;") && source.startsWith("L"))return true;
        ClassInfo internal=runtime.describe(source);String[] parents;
        if(internal!=null){List<String> list=new ArrayList<>(Arrays.asList(internal.interfaces));if(internal.parent!=null)list.add(internal.parent);parents=list.toArray(new String[0]);}
        else if(AndroidOperations.knownType(source))parents=AndroidOperations.parents(source);
        else {parents=hostParents(source);if(parents==null)throw new MissingOperation("type-relation:"+source+":"+target);}
        for(String parent:parents)if(ancestry(parent,target,seen))return true;return false;
    }
    public boolean isInstance(Object value,String type){
        if(value==null)return false;
        if(value instanceof Host)return isAssignable(((Host)value).type,type);
        String android=AndroidOperations.type(value);if(android!=null)return isAssignable(android,type);
        Class<?> cls=hostClass(type);if(cls==null)throw new MissingOperation("type:"+type);
        return cls.isInstance(value);
    }
    public Value call(ExecutableRuntime rt,String dispatch,String caller,String method,Value receiver,Value[] args)throws Throwable {
        Map<String,String> previous=boundary.get(), current=new LinkedHashMap<>();
        current.put("caller",caller==null?"":caller);current.put("foreignMethod",method);current.put("phase","foreign_call");
        boundary.set(current);
        try{return callBound(rt,dispatch,caller,method,receiver,args);}
        catch(Throwable error){
            // A reentrant outer call must not overwrite the innermost failure.
            boolean same=false;Throwable cursor=error;
            for(int depth=0;cursor!=null && depth<8;depth++){
                if(cursor==foreignError){same=true;break;}Throwable next=cursor.getCause();
                if((next==null || next==cursor) && cursor instanceof ExceptionInInitializerError)next=((ExceptionInInitializerError)cursor).getException();
                if(next==cursor)break;cursor=next;
            }
            if(!same){foreignError=error;foreignFailure=Collections.unmodifiableMap(new LinkedHashMap<>(current));}
            throw error;
        }finally{if(previous==null)boundary.remove();else boundary.set(previous);}
    }
    private Value callBound(ExecutableRuntime rt,String dispatch,String caller,String method,Value receiver,Value[] args)throws Throwable {
        boolean isStatic=callStatic(method);
        if(isStatic!=dispatch.equals("static"))throw new IncompatibleClassChangeError(method);
        String parameterText=method.substring(method.indexOf('(')+1,method.indexOf(')'));
        List<String> parameters=new ArrayList<>();
        for(int i=0;i<parameterText.length();){int start=i;while(parameterText.charAt(i)=='[')i++;i=parameterText.charAt(i)=='L'?parameterText.indexOf(';',i)+1:i+1;parameters.add(parameterText.substring(start,i));}
        if(args.length!=parameters.size())throw new InvalidProgram("Foreign argument count: "+method);
        for(int i=0;i<args.length;i++)rt.check(args[i],parameters.get(i));
        if(!isStatic){Objects.requireNonNull(receiver.o());String target=method.substring(0,method.indexOf("->"));rt.check(receiver,target);}
        if(method.startsWith("Landroid/"))return AndroidOperations.call(this,method,receiver,args);
        String owner=method.substring(0,method.indexOf("->")),signature=method.substring(method.indexOf("->")+2);
        if(owner.startsWith("[") && signature.equals("clone()Ljava/lang/Object;"))return runtime.cloneArray(receiver);
        if(owner.equals("Ljava/lang/Object;")){
            Object value=receiver.o();
            if(signature.equals("<init>()V")){if(value instanceof Host)construct(receiver,new Object());return Value.VOID;}
            if(signature.equals("clone()Ljava/lang/Object;")){if(value instanceof ArrayValue)return runtime.cloneArray(receiver);throw new MissingOperation("clone-receiver-model");}
            if(signature.equals("getClass()Ljava/lang/Class;"))return Value.ref(runtime.classToken(value instanceof Instance?((Instance)value).type:value instanceof ArrayValue?((ArrayValue)value).type:value instanceof Host?((Host)value).type:value instanceof ClassToken?"Ljava/lang/Class;":descriptor(value.getClass())));
            if(signature.equals("equals(Ljava/lang/Object;)Z"))return Value.i32(objectEquals(value,args[0].o())?1:0);
            if(signature.equals("hashCode()I"))return Value.i32(objectHash(value));
            if(signature.equals("toString()Ljava/lang/String;"))return Value.ref(objectString(value));
        }
        if(owner.equals("Ljava/lang/Class;") && receiver.o() instanceof ClassToken){
            String type=((ClassToken)receiver.o()).type;
            if(signature.equals("getName()Ljava/lang/String;"))return Value.ref(className(type));
            if(signature.equals("getSimpleName()Ljava/lang/String;"))return Value.ref(simpleName(type));
            if(signature.equals("getComponentType()Ljava/lang/Class;"))return type.startsWith("[")?Value.ref(runtime.classToken(type.substring(1))):Value.NULL;
        }
        if(method.equals("Ljava/lang/reflect/Array;->newInstance(Ljava/lang/Class;I)Ljava/lang/Object;")){
            ClassToken component=(ClassToken)Objects.requireNonNull(args[0].o());if(component.type.equals("V"))throw new IllegalArgumentException("Void component");
            return runtime.newArray("["+component.type,args[1].i());
        }
        if(method.equals("Ljava/util/Arrays;->copyOf([Ljava/lang/Object;I)[Ljava/lang/Object;")){
            ArrayValue original=(ArrayValue)Objects.requireNonNull(args[0].o());Value copy=runtime.newArray(original.type,args[1].i());
            System.arraycopy(original.values,0,((ArrayValue)copy.o()).values,0,Math.min(original.values.length,args[1].i()));return copy;
        }
        if(owner.equals("Ljava/lang/Enum;"))return enumCall(signature,receiver,args);
        if(owner.equals("Ljava/lang/Comparable;") && receiver.o() instanceof Instance && ((Instance)receiver.o()).foreignState.containsKey("Ljava/lang/Enum;"))
            return enumCall(signature,receiver,args);
        return jreCall(method,receiver,args);
    }
    public Value readField(String field,Value receiver)throws Throwable {
        if(field.startsWith("Landroid/"))return AndroidOperations.field(this,field,receiver);
        return jreField(field,receiver);
    }
    public void validateField(String field,boolean isStatic){if(fieldStatic(field)!=isStatic)throw new IncompatibleClassChangeError(field);}
    public void writeField(String field,Value receiver,Value value){throw new MissingOperation("write:"+field);}
    private static final class EnumState {String name;int ordinal;boolean initialized;}
    private EnumState enumeration(Object value){
        if(!(value instanceof Instance))throw new ClassCastException("Not a translated enum");
        Object state=((Instance)value).foreignState.get("Ljava/lang/Enum;");if(!(state instanceof EnumState))throw new InvalidProgram("Enum superclass not prepared");return (EnumState)state;
    }
    private Value enumCall(String signature,Value receiver,Value[] args)throws Throwable {
        if(signature.equals("valueOf(Ljava/lang/Class;Ljava/lang/String;)Ljava/lang/Enum;")){
            ClassToken cls=(ClassToken)args[0].o();String name=(String)args[1].o();if(cls==null || name==null)throw new NullPointerException();
            if(!runtime.isAssignable(cls.type,"Ljava/lang/Enum;"))throw new IllegalArgumentException("Not an enum: "+cls.type);
            Value values=runtime.invoke("static",null,cls.type+"->values()["+cls.type,Value.NULL,new Value[0]);
            for(Value value:((ArrayValue)values.o()).values)if(name.equals(enumeration(value.o()).name))return value;
            throw new IllegalArgumentException("No enum constant "+cls.type+"."+name);
        }
        EnumState state=enumeration(receiver.o());
        if(signature.equals("<init>(Ljava/lang/String;I)V")){if(state.initialized)throw new InvalidProgram("Enum constructed twice");state.name=(String)args[0].o();state.ordinal=args[1].i();state.initialized=true;return Value.VOID;}
        if(!state.initialized)throw new InvalidProgram("Unconstructed enum");
        if(signature.equals("name()Ljava/lang/String;") || signature.equals("toString()Ljava/lang/String;"))return Value.ref(state.name);
        if(signature.equals("ordinal()I"))return Value.i32(state.ordinal);
        if(signature.equals("equals(Ljava/lang/Object;)Z"))return Value.i32(receiver.o()==args[0].o()?1:0);
        if(signature.equals("hashCode()I"))return Value.i32(System.identityHashCode(receiver.o()));
        if(signature.equals("compareTo(Ljava/lang/Object;)I") || signature.equals("compareTo(Ljava/lang/Enum;)I")){
            Instance first=(Instance)receiver.o(),second=(Instance)Objects.requireNonNull(args[0].o());
            String a=first.type,b=second.type;
            while(runtime.describe(a)!=null && !"Ljava/lang/Enum;".equals(runtime.describe(a).parent))a=runtime.describe(a).parent;
            while(runtime.describe(b)!=null && !"Ljava/lang/Enum;".equals(runtime.describe(b).parent))b=runtime.describe(b).parent;
            if(!Objects.equals(a,b))throw new ClassCastException("Different enum classes");return Value.i32(state.ordinal-enumeration(second).ordinal);
        }
        throw new MissingOperation("Enum."+signature);
    }
    private String simpleName(String type){
        if(type.startsWith("["))return simpleName(type.substring(1))+"[]";
        if(type.length()==1)return className(type);
        Class<?> host=hostClass(type);if(host!=null)return host.getSimpleName();
        if(runtime.describe(type)==null)throw new MissingOperation("class-name:"+type);
        return runtime.describe(type).simpleName;
    }
    private static String className(String type){
        if(type.startsWith("["))return type.replace('/','.');
        switch(type){case "Z":return "boolean";case "B":return "byte";case "C":return "char";case "S":return "short";case "I":return "int";case "J":return "long";case "F":return "float";case "D":return "double";case "V":return "void";default:return type.substring(1,type.length()-1).replace('/','.');}
    }
    private String ownMethod(Object object,String signature){
        if(!(object instanceof Instance))return null;
        String method=runtime.method(((Instance)object).type,signature);
        if(method==null)return null;
        if(!runtime.hasBody(method))throw new MissingOperation("callback-body:"+method);
        return method;
    }
    private boolean objectEquals(Object first,Object second)throws Throwable {
        String method=ownMethod(first,"equals(Ljava/lang/Object;)Z");
        if(method!=null)return runtime.invoke("virtual",null,method,Value.ref(first),new Value[]{Value.ref(second)}).z();
        if(first instanceof Instance || first instanceof ArrayValue || first instanceof ClassToken)return first==second;
        return Objects.equals(exportObject(first,"Ljava/lang/Object;"),exportObject(second,"Ljava/lang/Object;"));
    }
    private int objectHash(Object value)throws Throwable {
        String method=ownMethod(value,"hashCode()I");if(method!=null)return runtime.invoke("virtual",null,method,Value.ref(value),new Value[0]).i();
        if(value instanceof ClassToken){Class<?> cls=hostClass(((ClassToken)value).type);return cls==null?System.identityHashCode(value):cls.hashCode();}
        return value instanceof Instance || value instanceof ArrayValue?System.identityHashCode(value):exportObject(value,"Ljava/lang/Object;").hashCode();
    }
    private String objectString(Object value)throws Throwable {
        if(value==null)return "null";
        String method=ownMethod(value,"toString()Ljava/lang/String;");if(method!=null)return (String)runtime.invoke("virtual",null,method,Value.ref(value),new Value[0]).o();
        if(value instanceof Instance){Instance instance=(Instance)value;if(instance.foreignState.containsKey("Ljava/lang/Enum;"))return enumeration(value).name;return instance.type.substring(1,instance.type.length()-1).replace('/','.')+"@"+Integer.toHexString(System.identityHashCode(value));}
        if(value instanceof ArrayValue)return ((ArrayValue)value).type.replace('/','.')+"@"+Integer.toHexString(System.identityHashCode(value));
        if(value instanceof ClassToken){String type=((ClassToken)value).type;Class<?> cls=hostClass(type);if(cls!=null)return cls.toString();ClassInfo source=runtime.describe(type);return (type.length()==1?"":source!=null && source.isInterface()?"interface ":"class ")+className(type);}
        return exportObject(value,"Ljava/lang/Object;").toString();
    }
    public Value callback(Instance source,String descriptor,Value[] args){
        try{return runtime.invoke("interface",null,descriptor,Value.ref(source),args);}catch(Throwable error){return RuntimeForeign.<RuntimeException,Value>raise(error);}
    }
    private static <E extends Throwable,T> T raise(Throwable error)throws E {throw (E)error;}
    public static class SourceProxy {
        protected final RuntimeForeign owner;protected final Instance source;
        SourceProxy(RuntimeForeign owner,Instance source){this.owner=owner;this.source=source;}
        public boolean equals(Object other){try{return owner.objectEquals(source,owner.importValue(other).o());}catch(Throwable error){return RuntimeForeign.<RuntimeException,Boolean>raise(error);}}
        public int hashCode(){try{return owner.objectHash(source);}catch(Throwable error){return RuntimeForeign.<RuntimeException,Integer>raise(error);}}
        public String toString(){try{return owner.objectString(source);}catch(Throwable error){return RuntimeForeign.<RuntimeException,String>raise(error);}}
    }
    static final class Context {
        final Resources resources;
        Context(Resources resources){this.resources=resources;}
    }
    static final class Looper {}
    static final class Handler {Looper looper;}
    static final class PackageManager {}
    static final class PackageFlags {final long bits;PackageFlags(long bits){this.bits=bits;}}
    static final class PackageInfo {final String version;PackageInfo(String version){this.version=version;}}
    static final class Configuration {
        Locale locale; final Map<String,Integer> numbers=new LinkedHashMap<>(); boolean initialized;
        Configuration(){}
        Configuration(Configuration source){
            if(!source.initialized)throw new InvalidProgram("Configuration copied before initialization");
            locale=source.locale==null?null:(Locale)source.locale.clone();numbers.putAll(source.numbers);initialized=true;
        }
    }
    static final class Resources {
        Configuration configuration;
        Resources(){}
        Resources(Configuration config){configuration=new Configuration(config);}
    }
    static final class LocaleList {
        final Locale[] locales;
        LocaleList(Locale locale){locales=locale==null?new Locale[0]:new Locale[]{locale};}
    }
    static final class Toast {final String message;final int duration;Toast(String message,int duration){this.message=message;this.duration=duration;}}
    static final class Preferences {
        final RuntimeForeign owner;final String namespace;final Map<String,String> current=new LinkedHashMap<>();
        Preferences(RuntimeForeign owner,String ns){this.owner=owner;namespace=ns;}
        synchronized String read(String key){return current.containsKey(key)?current.get(key):owner.services.preference(namespace,key);}
        synchronized boolean commit(Map<String,String> changes){current.putAll(changes);return owner.services.commitPreferences(namespace,new LinkedHashMap<>(current));}
    }
    static final class Editor {
        final Preferences preferences;final Map<String,String> changes=new LinkedHashMap<>();Editor(Preferences p){preferences=p;}
        synchronized Editor put(String key,String value){changes.put(Objects.requireNonNull(key),value);return this;}
        synchronized boolean commit(){Map<String,String> copy=new LinkedHashMap<>(changes);changes.clear();return preferences.commit(copy);}
    }
    synchronized Preferences preferences(String name){return preferences.computeIfAbsent(Objects.requireNonNull(name),n->new Preferences(this,n));}
    Context contextObject(){return context;}
    Looper mainLooper(){return main;}
}
