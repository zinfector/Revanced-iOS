"""Generate generic runtime initialization and a source-key diagnostic setting."""
import json
from pathlib import Path

from ..emitter.symbols import parse_descriptor


def q(value):
    return json.dumps(value, ensure_ascii=True)


def java_source(emission, configuration, setting, context_root, policy):
    entries = {e["descriptor"]: e for e in emission["entry_points"]}
    bootstrap = entries[context_root]; decision = entries[policy]
    if (bootstrap["parameters"] != ["Landroid/content/Context;"] or bootstrap["result"] != "V"
            or not bootstrap["static_source"] or not decision["static_source"] or decision["parameters"] or decision["result"] != "Z"):
        raise ValueError("PoC bootstrap/policy entry signature mismatch")
    profile = dict(configuration["profile"]); resources = configuration.get("resources", {})
    for identifier, color in resources.get("colors", {}).items():
        profile["resource.color." + str(int(identifier))] = str(int(color))
    for locale, strings in resources.get("localized_strings", {}).items():
        for identifier, value in strings.items():
            profile["resource.string." + locale + "." + str(int(identifier))] = value
    factories = []
    def table(name, values, typ, encode):
        pairs = sorted(values.items(), key=lambda v: str(v[0])); calls = []
        for start in range(0, len(pairs), 128):
            helper = name + str(start // 128); calls.append(helper + "(values);")
            body = "\n".join("values.put(" + (str(int(k)) if name == "strings" else q(k)) + "," + encode(v) + ");" for k, v in pairs[start:start + 128])
            factories.append("private static void " + helper + "(Map<" + typ + "> values){" + body + "}\n")
        factories.append("private static Map<" + typ + "> " + name + "(){Map<" + typ + "> values=new LinkedHashMap<>();" + "".join(calls) + "return values;}\n")
    table("profile", profile, "String,String", q)
    table("ids", resources.get("ids", {}), "String,Integer", lambda n: str(int(n)))
    table("strings", resources.get("strings", {}), "Integer,String", q)
    source = '''package rv.executable;
import java.util.*;
/** Fresh generated initialization; no direct writes into translated source fields. */
public final class GeneratedPocBootstrap {
 private static IOSPlatformServices services;
 private static int state; // 0 not started, 1 initializing, 2 ready, 3 failed
 private static String failure="";
 private static String stage="not_started";
 private static GeneratedForeign provider;
 private static Throwable originalFailure;
 private static Map<String,Object> failureDetails=Collections.emptyMap();
 private static String bounded(String text,int limit){return text==null?"":text.length()>limit?text.substring(0,limit):text;}
 private static Map<String,Object> describeFailure(Throwable error){
  Map<String,Object> details=new LinkedHashMap<>();details.put("stage",stage);
  List<Object> chain=new ArrayList<>();Set<Throwable> seen=Collections.newSetFromMap(new IdentityHashMap<>());
  for(Throwable cursor=error;cursor!=null && seen.add(cursor) && chain.size()<8;){
   Map<String,Object> row=new LinkedHashMap<>();row.put("type",cursor.getClass().getName());row.put("message",bounded(cursor.getMessage(),512));
   List<String> stack=new ArrayList<>();StackTraceElement[] frames=cursor.getStackTrace();
   for(int i=0;i<frames.length && i<12;i++)stack.add(bounded(frames[i].toString(),256));
   row.put("stack",stack);chain.add(row);
   Throwable next=cursor.getCause();
   if((next==null || next==cursor) && cursor instanceof ExceptionInInitializerError)next=((ExceptionInInitializerError)cursor).getException();
   cursor=next;
  }
  details.put("causes",chain);if(provider!=null)details.put("foreignBoundary",provider.failureContext());
  try{details.put("failedSourceClasses",GeneratedEntryPoints.runtime().failedClasses());}catch(Throwable ignored){}
  return details;
 }
 private static synchronized IOSPlatformServices services(){
  if(services==null)services=new IOSPlatformServices(profile(),ids(),strings());return services;
 }
 public static synchronized boolean initializeRuntime(){
  if(state==2)return true;if(state!=0)return false;state=1;
  try {
   stage="array_runtime";ArrayInterop.prepare();
   stage="configure";provider=new GeneratedForeign(services());
   GeneratedEntryPoints.configure(provider);
   stage="source_context";
   GeneratedEntryPoints.CONTEXT(provider.context().o());
   stage="source_policy";
   GeneratedEntryPoints.POLICY(); // Preserve original active-use/static initialization.
   stage="ready";state=2;return true;
  } catch(Throwable error){
   originalFailure=error;failure=bounded(error.toString(),1024);state=3;
   try{failureDetails=describeFailure(error);}catch(Throwable diagnosticsError){failureDetails=Collections.singletonMap("stage",stage);}
   return false;
  }
 }
 public static synchronized String status(){return state==2?"ready":state==3?"failed: "+failure:state==1?"initializing":"unconfigured";}
 public static synchronized Map<String,Object> diagnostics(){
  Map<String,Object> report=new LinkedHashMap<>();report.put("state",state);report.put("stage",stage);report.put("failure",failureDetails);return report;
 }
 public static boolean readSetting(){
  String value=services().preference(NAMESPACE,KEY);
  if(value==null)return DEFAULT;
  if(!value.equals("Z:0") && !value.equals("Z:1"))throw new IllegalStateException("Invalid Boolean preference");
  return value.equals("Z:1");
 }
 public static boolean writeSetting(boolean value){
  return services().commitPreferences(NAMESPACE,Collections.singletonMap(KEY,value?"Z:1":"Z:0"));
 }
'''
    source = source.replace("GeneratedEntryPoints.CONTEXT", "GeneratedEntryPoints." + bootstrap["java_method"])
    source = source.replace("GeneratedEntryPoints.POLICY", "GeneratedEntryPoints." + decision["java_method"])
    source = source.replace("NAMESPACE", q(setting["namespace"])).replace("KEY", q(setting["key"])).replace("DEFAULT", str(setting["default"]).lower())
    return source + "\n".join(factories) + "}\n"


def native_source(headers, plan):
    """Discover fresh C exports after J2ObjC translation, including backend renaming."""
    import re
    header = Path(headers) / "rv/executable/GeneratedPocBootstrap.h"
    text = header.read_text(encoding="utf-8"); names = {}
    for method, result, arguments in [("initializeRuntime", "bool", "void"), ("status", "NSString *", "void"),
                                      ("diagnostics", "id<JavaUtilMap>", "void"),
                                      ("readSetting", "bool", "void"), ("writeSettingWithBoolean_", "bool", "bool")]:
        declarations = re.findall(r"FOUNDATION_EXPORT\s+([^;]*\b(\w+_" + method + r")\(([^;]*)\));", text)
        if len(declarations) != 1:
            raise ValueError("Fresh bootstrap declaration missing: " + method)
        declaration, symbol, formal = declarations[0]
        from ..emitter.symbols import canonical_c
        ret = declaration[:declaration.index(symbol)].strip()
        parameter = re.sub(r"\b\w+$", "", formal).strip() if arguments != "void" else formal.strip()
        if canonical_c(ret) != canonical_c(result) or canonical_c(parameter) != canonical_c(arguments):
            raise ValueError("Fresh bootstrap declaration ABI differs: " + method)
        names[method] = symbol
    array_header=(Path(headers)/"rv/executable/ArrayInterop.h").read_text(encoding="utf-8")
    array_exports=re.findall(r"FOUNDATION_EXPORT\s+NSString\s*\*\s*(\w+_snapshotJSON)\(void\);",array_header)
    if len(array_exports)!=1:raise ValueError("Fresh array diagnostics export missing")
    mode = plan["mode"]; expected = 1 if mode in ("active", "diagnostic") else 0
    install = "RVGInstall()" if mode == "active" else "RVGInstallObservations()" if mode == "diagnostic" else "0"
    init = names["initializeRuntime"] + "()" if mode == "active" else "true"
    source = '''#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include "rv/executable/GeneratedPocBootstrap.h"
#include "rv/executable/ArrayInterop.h"
#include "java/util/Map.h"
#include "java/util/Set.h"
#include "java/util/Iterator.h"
#include "java/util/List.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
extern unsigned RVGInstall(void);
extern void RVGSetEnabled(bool);
extern NSDictionary *RVGGuardReport(void);
extern unsigned RVGInstallObservations(void);
extern NSArray *RVGObservationSnapshot(void);
static NSString *installation=@"not_started";
// The existing image-bound debug.read transport reads this bounded snapshot.
char RVPOCReportBuffer[32768];
_Atomic(uint64_t) RVPOCReportGeneration=0;
_Atomic(uint32_t) RVPOCReportLength=0;
static id RVPOCFoundation(id value,unsigned depth) {
 if(!value || depth>8)return NSNull.null;
 if([value isKindOfClass:NSString.class] || [value isKindOfClass:NSNumber.class])return value;
 if([value conformsToProtocol:@protocol(JavaUtilMap)]){
  NSMutableDictionary *result=[NSMutableDictionary dictionary];id<JavaUtilMap> map=value;
  id<JavaUtilIterator> keys=[[map keySet] iterator];unsigned count=0;
  while([keys hasNext] && count++<64){id key=[keys next];if([key isKindOfClass:NSString.class])result[key]=RVPOCFoundation([map getWithId:key],depth+1);}
  return result;
 }
 if([value conformsToProtocol:@protocol(JavaUtilList)]){
  NSMutableArray *result=[NSMutableArray array];id<JavaUtilList> list=value;
  for(jint i=0;i<[list size] && i<32;i++)[result addObject:RVPOCFoundation([list getWithInt:i],depth+1)];return result;
 }
 return @"unsupported_diagnostic_value";
}
NSDictionary *RVPOCReport(void) {
 @try { return @{@"mode":@MODE, @"installation":installation, @"runtime":STATUS(),
  @"startup":RVPOCFoundation(DIAGNOSTICS(),0),@"arrayMetadata":ARRAY_SNAPSHOT,
  @"settingKey":@SETTING, @"restartRequired":@RESTART, @"guard":RVGGuardReport(),
  @"observations":RVGObservationSnapshot()}; }
 @catch(NSException *error){ return @{@"installation":installation,@"error":error.description}; }
}
static void RVPOCPublish(void) {
 @try {
  NSData *data=[NSJSONSerialization dataWithJSONObject:RVPOCReport() options:0 error:NULL];
  if(!data || data.length>=sizeof(RVPOCReportBuffer))data=[NSJSONSerialization dataWithJSONObject:@{@"error":@"diagnostic_snapshot_too_large"} options:0 error:NULL];
  atomic_fetch_add(&RVPOCReportGeneration,1);
  memcpy(RVPOCReportBuffer,data.bytes,data.length);RVPOCReportBuffer[data.length]=0;
  atomic_store(&RVPOCReportLength,(uint32_t)data.length);atomic_fetch_add(&RVPOCReportGeneration,1);
 } @catch(NSException *ignored){}
}
bool RVPOCWriteSetting(bool value) { @try { return WRITE(value); } @catch(NSException *error){NSLog(@"[GeneratedPoC] setting failure: %@",error);return false;} }
bool RVPOCPresentSettings(UIViewController *presenter) {
 if(!NSThread.isMainThread || !presenter || !presenter.viewIfLoaded.window || presenter.presentedViewController)return false;
 @try {
  BOOL current=READ();
  NSString *message=[NSString stringWithFormat:@"Current value: %@. Changes require restarting the app.",current?@"ON":@"OFF"];
  UIAlertController *dialog=[UIAlertController alertControllerWithTitle:@SETTING message:message preferredStyle:UIAlertControllerStyleAlert];
  [dialog addAction:[UIAlertAction actionWithTitle:current?@"Turn OFF":@"Turn ON" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){
   BOOL saved=RVPOCWriteSetting(!current);
   UIAlertController *notice=[UIAlertController alertControllerWithTitle:saved?@"Setting saved":@"Setting could not be saved"
    message:saved?@"Restart the app to apply this setting.":@"The preference service reported a write failure." preferredStyle:UIAlertControllerStyleAlert];
   [notice addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleCancel handler:nil]];
   [presenter presentViewController:notice animated:YES completion:nil];
  }]];
  [dialog addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
  [presenter presentViewController:dialog animated:YES completion:nil];return true;
 } @catch(NSException *error){NSLog(@"[GeneratedPoC] settings presentation failure: %@",error);return false;}
}
// Generic Objective-C access for debugger transports that can invoke reviewed selectors.
@interface RVGeneratedPocDiagnostics : NSObject
+ (NSDictionary *)report;
+ (BOOL)writeSetting:(BOOL)value;
+ (BOOL)presentSettings:(UIViewController *)presenter;
@end
@implementation RVGeneratedPocDiagnostics
+ (NSDictionary *)report { return RVPOCReport(); }
+ (BOOL)writeSetting:(BOOL)value { return RVPOCWriteSetting(value); }
+ (BOOL)presentSettings:(UIViewController *)presenter { return RVPOCPresentSettings(presenter); }
@end
static void activate(unsigned attempts) {
 if(!INIT){installation=@"initialization_failed";RVPOCPublish();NSLog(@"[GeneratedPoC] %@",STATUS());return;}
 unsigned count=INSTALL;
 if(count==EXPECTED && count>0){ENABLED installation=@"installed";RVPOCPublish();return;}
 installation=@"waiting_for_slot";
 if(attempts)dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{activate(attempts-1);});
 else installation=@"slot_installation_failed";
 RVPOCPublish();
}
bool RVPOCInitializeInspection(void) { return INITIALIZE(); }
'''
    replacements = {"MODE": q(mode), "SETTING": q(plan["setting"]["key"]), "RESTART": str(plan["setting"]["restart_required"]).lower(),
                    "DIAGNOSTICS()": names["diagnostics"] + "()",
                    "ARRAY_SNAPSHOT": "([NSJSONSerialization JSONObjectWithData:[" + array_exports[0] + "() dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL] ?: @[])",
                    "STATUS()": names["status"] + "()", "READ()": names["readSetting"] + "()",
                    "WRITE(value)": names["writeSettingWithBoolean_"] + "(value)", "INIT": init,
                    "INSTALL": install, "EXPECTED": str(expected), "ENABLED": "RVGSetEnabled(true);" if mode == "active" else "",
                    "INITIALIZE()": names["initializeRuntime"] + "()"}
    # Token replacement avoids replacing INIT within INITIALIZE.
    for token in sorted(replacements, key=len, reverse=True):
        source = re.sub(r"(?<![A-Za-z0-9_])" + re.escape(token) + r"(?![A-Za-z0-9_])", lambda _: replacements[token], source)
    if mode != "inspection":
        source += """__attribute__((constructor)) static void load(void){dispatch_async(dispatch_get_main_queue(),^{
 activate(60);
 static dispatch_source_t publisher;
 publisher=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_get_main_queue());
 dispatch_source_set_timer(publisher,dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC),2*NSEC_PER_SEC,NSEC_PER_SEC/4);
 dispatch_source_set_event_handler(publisher,^{RVPOCPublish();});dispatch_resume(publisher);
});}
"""
    return source
