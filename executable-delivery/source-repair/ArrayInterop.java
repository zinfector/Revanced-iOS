package rv.executable;

/*-[
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include "IOSArray.h"
#include "IOSObjectArray.h"
#include "IOSClass.h"
#include "java/lang/IllegalStateException.h"

static NSMutableArray *RVEArrayMetadataEvents;
static IOSClass *RVECanonicalArrayClass(id object, SEL selector) {
  // Exact pinned IOSArray layout/API; no guessing based on a class name.
  return IOSClass_arrayOf([(IOSArray *)object elementType]);
}
static NSString *RVEArrayImage(const void *pointer) {
  Dl_info info={0};
  return dladdr(pointer,&info) && info.dli_fname ?
    [[NSString stringWithUTF8String:info.dli_fname] lastPathComponent] : @"unknown";
}
static BOOL RVEArrayOwns(const void *pointer) {
  Dl_info own={0}, other={0};
  return dladdr((const void *)&RVECanonicalArrayClass,&own) && dladdr(pointer,&other)
    && own.dli_fbase && own.dli_fbase==other.dli_fbase;
}
static BOOL RVEArrayGetterABI(Method method) {
  if(!method || method_getNumberOfArguments(method)!=2)return NO;
  char *ret=method_copyReturnType(method), *selfType=method_copyArgumentType(method,0), *selType=method_copyArgumentType(method,1);
  BOOL valid=ret && !strcmp(ret,"@") && selfType && !strcmp(selfType,"@") && selType && !strcmp(selType,":");
  free(ret);free(selfType);free(selType);return valid;
}
static BOOL RVEOwnedArrayReferentSubclass(Class cls, IOSClass *actual) {
  // Pinned IOSReference.m creates this class on the heap: dladdr(cls) cannot
  // establish its origin. Admit its exact construction shape and owned IMPs,
  // not its name alone. Keep its release/dealloc reference bookkeeping intact.
  Class parent=class_getSuperclass(cls);
  if(!parent || class_getImageName(cls) || !RVEArrayOwns((__bridge const void *)parent)
     || class_getInstanceSize(cls)!=class_getInstanceSize(parent))return NO;
  BOOL arrayParent=NO;
  for(Class c=parent;c;c=class_getSuperclass(c))if(c==[IOSArray class]){arrayParent=YES;break;}
  if(!arrayParent)return NO;
  NSString *name=[NSString stringWithFormat:@"%s_ReferentSubclass",class_getName(parent)];
  if(strcmp(class_getName(cls),[name UTF8String]))return NO;
  unsigned int ivarCount=0,methodCount=0;
  Ivar *ivars=class_copyIvarList(cls,&ivarCount);free(ivars);
  Method *methods=class_copyMethodList(cls,&methodCount);
  BOOL valid=!ivarCount && methodCount==3;
  BOOL getter=NO,release=NO,dealloc=NO;
  for(unsigned int i=0;i<methodCount;i++){
    SEL selector=method_getName(methods[i]);
    if(sel_isEqual(selector,@selector(java_getClass))){getter=YES;valid=valid && RVEArrayGetterABI(methods[i]);}
    else if(sel_isEqual(selector,sel_registerName("release")))release=YES;
    else if(sel_isEqual(selector,sel_registerName("dealloc")))dealloc=YES;
    else valid=NO;
    valid=valid && RVEArrayOwns((const void *)method_getImplementation(methods[i]));
  }
  free(methods);
  Method parentGetter=class_getInstanceMethod(parent,@selector(java_getClass));
  return valid && getter && release && dealloc && RVEArrayGetterABI(parentGetter)
    && RVEArrayOwns((const void *)method_getImplementation(parentGetter))
    && actual==IOSClass_fromClass(parent);
}
static void RVEEnsureArrayMetadata(id object) {
  if(!object || ![object isKindOfClass:[IOSArray class]])
    @throw create_JavaLangIllegalStateException_initWithNSString_(@"Array boundary received a non-IOSArray");
  @synchronized([IOSArray class]) {
    Class cls=object_getClass(object);
    IOSClass *component=[(IOSArray *)object elementType];
    Method method=class_getInstanceMethod(cls,@selector(java_getClass));
    if(!component || ![component isKindOfClass:[IOSClass class]] || !RVEArrayGetterABI(method))
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Invalid native array component/getter ABI");
    IOSClass *expected=IOSClass_arrayOf(component);
    IOSClass *actual=[object java_getClass];
    BOOL consistent=actual==expected && [actual isArray] && [actual getComponentType]==component;
    IMP previous=method_getImplementation(method);
    NSMutableDictionary *event=[@{@"nativeClass":NSStringFromClass(cls),
      @"superclass":NSStringFromClass(class_getSuperclass(cls)) ?: @"",
      @"classImage":RVEArrayImage((__bridge const void *)cls),@"getterImage":RVEArrayImage((const void *)previous),
      @"returnedClass":actual ? NSStringFromClass(object_getClass(actual)) : @"nil",
      @"isArray":@([actual isArray]),@"factoryClass":expected ? NSStringFromClass(object_getClass(expected)) : @"nil",
      @"factoryIsArray":@([expected isArray]),@"phase":consistent ? @"metadata_ready" : @"metadata_mismatch",@"repaired":@NO} mutableCopy];
    if(!RVEArrayMetadataEvents)RVEArrayMetadataEvents=[NSMutableArray array];
    if((!consistent || !RVEArrayMetadataEvents.count) && RVEArrayMetadataEvents.count<16)[RVEArrayMetadataEvents addObject:event];
    Class arrayClass=objc_getClass("IOSArrayClass");
    if(!arrayClass || ![expected isKindOfClass:arrayClass] || ![expected isArray] || [expected getComponentType]!=component){
      event[@"phase"]=@"array_class_factory_inconsistent";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Pinned array-class factory is inconsistent");
    }
    if(consistent)return;
    BOOL ownedReferent=RVEOwnedArrayReferentSubclass(cls,actual);
    event[@"ownedReferentSubclass"]=@(ownedReferent);
    // A pinned, dynamically allocated reference subclass has no image address;
    // its parent, layout, own method set and implementation owners prove origin.
    // All other foreign classes remain refused.
    if((!RVEArrayOwns((__bridge const void *)cls) && !ownedReferent) || !RVEArrayOwns((__bridge const void *)[IOSArray class])
       || !RVEArrayOwns((__bridge const void *)arrayClass)
       || !RVEArrayOwns((__bridge const void *)object_getClass(component)) || !RVEArrayOwns((const void *)previous)) {
      event[@"phase"]=@"foreign_runtime_ownership";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Array runtime belongs to another image; repair refused");
    }
    // Create a concrete-class override rather than modifying an inherited Method.
    // The replacement computes the real component-specific class; it does not
    // force isArray or weaken component assignment/cast checks.
    const char *encoding=method_getTypeEncoding(method);
    if(!class_addMethod(cls,@selector(java_getClass),(IMP)RVECanonicalArrayClass,encoding))
      class_replaceMethod(cls,@selector(java_getClass),(IMP)RVECanonicalArrayClass,encoding);
    actual=[object java_getClass];
    if(actual!=expected || ![actual isArray] || [actual getComponentType]!=component) {
      class_replaceMethod(cls,@selector(java_getClass),previous,encoding);
      event[@"phase"]=@"repair_failed";
      @throw create_JavaLangIllegalStateException_initWithNSString_(@"Native array metadata repair failed");
    }
    event[@"phase"]=ownedReferent ? @"referent_array_getter_installed" : @"canonical_array_getter_installed";event[@"repaired"]=@YES;
  }
}
]-*/

/** Reusable pinned-backend admission at the source/native array boundary. */
public final class ArrayInterop {
    private ArrayInterop() {}
    public static void prepare() { ensure(new Object[0]); }
    public static boolean isArray(Object object) {
        if(object==null)return false;
        try { if(!isNativeArray(object))return false;ensureNative(object);return true; }
        catch(UnsatisfiedLinkError absentBackend){return object.getClass().isArray();}
    }
    private static native boolean isNativeArray(Object object) /*-[ return [object isKindOfClass:[IOSArray class]]; ]-*/;
    public static void ensure(Object array) {
        try { ensureNative(array); }
        catch(UnsatisfiedLinkError absentBackend) {
            if(array==null || !array.getClass().isArray())throw new IllegalArgumentException("Not a Java array");
        }
    }
    private static native void ensureNative(Object array) /*-[ RVEEnsureArrayMetadata(array); ]-*/;
    public static String snapshotJSON(){try{return snapshotNative();}catch(UnsatisfiedLinkError absentBackend){return "[]";}}
    private static native String snapshotNative() /*-[
      @synchronized([IOSArray class]) {
        NSData *data=[NSJSONSerialization dataWithJSONObject:RVEArrayMetadataEvents ?: @[] options:0 error:NULL];
        return data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"[]";
      }
    ]-*/;
}
