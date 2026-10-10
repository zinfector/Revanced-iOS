// SPDX-License-Identifier: MIT
// Explicit, foreground-only diagnostics over a device loopback TCP socket.
// The PC reaches this port through Apple's USB multiplexer, not a LAN listener.
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#import <Security/Security.h>
#include "RVDebugAgent.inc"

enum { RVUSBPort=49629,RVUSBRequestLimit=8192,RVUSBResponseLimit=16*1024*1024,RVUSBClientLimit=4 };
static NSString *const RVUSBEnabledPreference=@"rvport.usb-diagnostics.enabled";
static NSDictionary *RVUSBPairingQuery(void) {
    // Omit access-group: the signer supplies the app's default private group.
    return @{(__bridge id)kSecClass:(__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService:@"RVPort USB diagnostics",(__bridge id)kSecAttrAccount:@"trusted-computers-v1"};
}
static NSMutableDictionary *RVUSBLoadPairings(OSStatus *status) {
    NSMutableDictionary *query=[RVUSBPairingQuery() mutableCopy];
    query[(__bridge id)kSecReturnData]=@YES;query[(__bridge id)kSecMatchLimit]=(__bridge id)kSecMatchLimitOne;
    CFTypeRef result=NULL;*status=SecItemCopyMatching((__bridge CFDictionaryRef)query,&result);
    if (*status==errSecItemNotFound) { *status=errSecSuccess;return [NSMutableDictionary dictionary]; }
    if (*status!=errSecSuccess) return nil;
    NSData *data=CFBridgingRelease(result);
    id value=[data isKindOfClass:NSData.class] && data.length<16384 ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    id clients=[value isKindOfClass:NSDictionary.class] ? value[@"clients"] : nil;
    if (![clients isKindOfClass:NSDictionary.class] || [clients count]>8) { *status=errSecDecode;return nil; }
    for (id key in clients) {
        id record=clients[key];id token=[record isKindOfClass:NSDictionary.class] ? record[@"token"] : nil;
        id name=[record isKindOfClass:NSDictionary.class] ? record[@"name"] : nil;
        if (![key isKindOfClass:NSString.class] || ![token isKindOfClass:NSString.class] || [token length]!=32 || ![name isKindOfClass:NSString.class]) { *status=errSecDecode;return nil; }
    }
    return [clients mutableCopy];
}
static OSStatus RVUSBSavePairings(NSDictionary *clients) {
    if (!clients.count) { OSStatus status=SecItemDelete((__bridge CFDictionaryRef)RVUSBPairingQuery());return status==errSecItemNotFound ? errSecSuccess : status; }
    NSData *data=[NSJSONSerialization dataWithJSONObject:@{@"schema":@1,@"clients":clients} options:0 error:nil];
    if (!data || data.length>=16384) return errSecParam;
    NSDictionary *attributes=@{(__bridge id)kSecValueData:data,
        (__bridge id)kSecAttrAccessible:(__bridge id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly};
    NSMutableDictionary *item=[RVUSBPairingQuery() mutableCopy];[item addEntriesFromDictionary:attributes];
    OSStatus status=SecItemAdd((__bridge CFDictionaryRef)item,NULL);
    if (status==errSecDuplicateItem) status=SecItemUpdate((__bridge CFDictionaryRef)RVUSBPairingQuery(),(__bridge CFDictionaryRef)attributes);
    return status;
}
static NSDictionary *RVDiagnosticsSnapshot(NSString *topic) {
    if (!NSThread.isMainThread) return nil;
    NSDictionary *data=nil;
    if ([topic isEqual:@"dearrow"]) data=RVDAUIReport();
    else if ([topic isEqual:@"watch"]) data=RVDAWatchReport();
    else if ([topic isEqual:@"adblock"]) data=RVAdReport();
    else if ([topic isEqual:@"speed"]) data=RVSpeedReport();
    else if ([topic isEqual:@"ryd"]) data=RVDislikeCheckpointReport();
    else if ([topic isEqual:@"sponsorblock"]) data=RVSponsorReport();
    else if ([topic isEqual:@"navigation"]) data=RVShortsReport();
    else if ([topic isEqual:@"adaptive"]) data=@{@"profile":RVAdaptiveProfile[@"id"] ?: @"",
        @"catalog_sha256":RV_ADAPTIVE_CATALOG_SHA256,@"contracts":RVAdaptiveDiagnostics ?: @{},
        @"disabled_features":RVAdaptiveProfile[@"disabled_features"] ?: @[],@"runtime_disabled_features":RVAdaptiveBlockedFeatures.allObjects ?: @[],@"installed_layers":@(RVAdaptiveLedger.count)};
    else if ([topic isEqual:@"all"]) {
        NSArray *hooks;@synchronized(RVStatus) { hooks=RVStatus.copy; }
        data=@{@"effective_config":RVEffectivePreferences(),@"hooks":hooks ?: @[],@"authentication":RVAuthenticationReport(),
            @"sponsorblock":RVSponsorReport(),@"sponsorblock_prompts":RVSPReport(),@"shorts_layout":RVShortsReport(),
            @"return_youtube_dislike":RVNativeDislikeReport(),@"ryd_checkpoints":RVDislikeCheckpointReport(),
            @"playback_speed":RVSpeedReport(),@"adblock":RVAdReport(),@"dearrow":RVDAUIReport(),
            @"adaptive":@{@"profile":RVAdaptiveProfile[@"id"] ?: @"",@"contracts":RVAdaptiveDiagnostics ?: @{},
                @"disabled_features":RVAdaptiveProfile[@"disabled_features"] ?: @[],@"runtime_disabled_features":RVAdaptiveBlockedFeatures.allObjects ?: @[],@"installed_layers":@(RVAdaptiveLedger.count)}};
    }
    if (!data) return nil;
    NSMutableDictionary *report=[data mutableCopy];
    [report addEntriesFromDictionary:@{@"patcher_version":RV_PORT_VERSION,@"release_flavor":@"usb-diagnostics",
        @"build_source_sha256":RV_BUILD_SOURCE_SHA256,@"profile_accepted":@(RVCompatible),
        @"youtube_version":NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"] ?: @"",
        @"ios_version":UIDevice.currentDevice.systemVersion,@"device_model":UIDevice.currentDevice.model}];
    return report.copy;
}
static NSDictionary *RVUSBError(NSString *reason) { return @{@"schema":@1,@"ok":@NO,@"error":reason}; }
static BOOL RVUSBRead(int fd,void *buffer,size_t length) {
    uint8_t *bytes=buffer;size_t position=0;
    while (position<length) {
        ssize_t count=recv(fd,bytes+position,length-position,0);
        if (count<0 && errno==EINTR) continue;
        if (count<=0) return NO;position+=(size_t)count;
    }return YES;
}
static BOOL RVUSBWrite(int fd,const void *buffer,size_t length) {
    const uint8_t *bytes=buffer;size_t position=0;
    while (position<length) {
        ssize_t count=send(fd,bytes+position,length-position,0);
        if (count<0 && errno==EINTR) continue;
        if (count<=0) return NO;position+=(size_t)count;
    }return YES;
}
@interface RVUSBDiagnosticTicket : NSObject
@property(atomic) BOOL cancelled;
@property(nonatomic,strong) NSDictionary *result;
@end
@implementation RVUSBDiagnosticTicket @end

@interface RVUSBDiagnostics : NSObject
@property(nonatomic) BOOL enabled,ownsCapture,snapshotPending;
@property(nonatomic) BOOL resumeCapture,resumeScheduled;
@property(nonatomic) int listener;
@property(nonatomic) uint64_t generation;
@property(nonatomic) NSUInteger failedPairings;
@property(nonatomic,copy) NSString *accessCode,*sessionID;
@property(nonatomic,strong) NSMutableDictionary *pairings;
@property(nonatomic) OSStatus pairingStorageStatus;
@property(nonatomic,strong) NSMutableSet<NSNumber *> *clients;
@property(nonatomic,strong) dispatch_source_t source;
@property(nonatomic,strong) dispatch_queue_t ioQueue,clientQueue;
@property(nonatomic,strong) id backgroundObserver,foregroundObserver;
+ (instancetype)shared;
- (NSDictionary *)status;
- (NSString *)start;
- (void)stop;
- (void)pause;
- (void)resume;
- (NSString *)forgetComputers;
- (NSArray *)savedComputers;
- (void)acceptFrom:(int)listener generation:(uint64_t)generation;
- (void)serve:(int)fd generation:(uint64_t)generation;
- (NSDictionary *)process:(NSDictionary *)request generation:(uint64_t)generation;
@end
@implementation RVUSBDiagnostics
+ (instancetype)shared {
    static RVUSBDiagnostics *service;static dispatch_once_t once;
    dispatch_once(&once,^{
        service=[RVUSBDiagnostics new];service.listener=-1;service.clients=[NSMutableSet set];
        service.ioQueue=dispatch_queue_create("rvport.usb.accept",DISPATCH_QUEUE_SERIAL);
        service.clientQueue=dispatch_queue_create("rvport.usb.clients",DISPATCH_QUEUE_CONCURRENT);
        __weak RVUSBDiagnostics *weak=service;
        service.backgroundObserver=[[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidEnterBackgroundNotification
            object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) { [weak pause]; }];
        service.foregroundObserver=[[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
            object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *note) { [weak resume]; }];
    });return service;
}
- (NSDictionary *)status {
    @synchronized(self) {
        return @{@"schema":@1,@"enabled":@(self.enabled),@"port":@(RVUSBPort),@"transport":@"device_loopback_via_usbmux",
            @"session":self.sessionID ?: @"inactive",@"session_expires":@NO,
            @"resume_enabled":@([NSUserDefaults.standardUserDefaults boolForKey:RVUSBEnabledPreference]),
            @"saved_computers":@(self.pairings.count),@"pairing_storage_status":@(self.pairingStorageStatus),
            @"clients":@(self.clients.count),@"snapshot_pending":@(self.snapshotPending),
            @"patcher_version":RV_PORT_VERSION,@"build_source_sha256":RV_BUILD_SOURCE_SHA256,
            @"live_debugging_enabled":@(RVDBGEnabled()),@"debug_protocol_schema":@2,
            @"operations":@[@"status",@"report",@"capture-start",@"capture-stop",@"probe"]};
    }
}
- (NSString *)start {
    if (!NSThread.isMainThread || !RVCompatible || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return @"Open the supported YouTube app in the foreground first.";
    if (self.enabled) return nil;
    OSStatus storageStatus=errSecSuccess;NSMutableDictionary *pairings=RVUSBLoadPairings(&storageStatus);
    self.pairingStorageStatus=storageStatus;
    if (!pairings) return @"Could not read saved computers from the iPhone keychain. Unlock your iPhone and try again.";
    int fd=socket(AF_INET,SOCK_STREAM,0);if (fd<0) return @"Could not create the diagnostics socket.";
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in address={0};address.sin_len=sizeof(address);address.sin_family=AF_INET;
    address.sin_port=htons(RVUSBPort);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (fcntl(fd,F_SETFL,O_NONBLOCK)<0 || bind(fd,(struct sockaddr *)&address,sizeof(address))<0 || listen(fd,RVUSBClientLimit)<0) {
        close(fd);return @"Could not open the diagnostics port. Another session may still be closing; try again.";
    }
    dispatch_source_t source=dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,(uintptr_t)fd,0,self.ioQueue);
    if (!source) { close(fd);return @"Could not start the diagnostics listener."; }
    uint64_t generation;
    @synchronized(self) {
        self.listener=fd;self.enabled=YES;generation=++self.generation;self.failedPairings=0;
        self.accessCode=[[NSUUID.UUID.UUIDString stringByReplacingOccurrencesOfString:@"-" withString:@""] substringToIndex:8];
        self.pairings=pairings;self.sessionID=NSUUID.UUID.UUIDString;self.source=source;
    }
    __weak RVUSBDiagnostics *weak=self;
    dispatch_source_set_event_handler(source,^{ [weak acceptFrom:fd generation:generation]; });
    dispatch_source_set_cancel_handler(source,^{ close(fd); });dispatch_resume(source);
    [NSUserDefaults.standardUserDefaults setBool:YES forKey:RVUSBEnabledPreference];
    if (self.resumeCapture) { self.resumeCapture=NO;RVDAUIResumeCapture();self.ownsCapture=YES; }
    return nil;
}
- (void)stop {
    if (!NSThread.isMainThread) { __weak RVUSBDiagnostics *weak=self;dispatch_async(dispatch_get_main_queue(),^{ [weak stop]; });return; }
    [NSUserDefaults.standardUserDefaults setBool:NO forKey:RVUSBEnabledPreference];
    RVDBGSetEnabled(NO);
    [self pause];self.resumeCapture=NO;
}
- (void)resume {
    if (!NSThread.isMainThread || ![NSUserDefaults.standardUserDefaults boolForKey:RVUSBEnabledPreference] || UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) return;
    NSString *error=[self start];
    if (error && !self.resumeScheduled) {
        self.resumeScheduled=YES;__weak RVUSBDiagnostics *weak=self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,2*NSEC_PER_SEC),dispatch_get_main_queue(),^{
            weak.resumeScheduled=NO;[weak resume];
        });
    }
}
- (NSArray *)savedComputers {
    @synchronized(self) {
        NSMutableArray *names=[NSMutableArray array];
        for (NSDictionary *record in self.pairings.allValues) [names addObject:record[@"name"] ?: @"Paired computer"];
        return names.copy;
    }
}
- (NSString *)forgetComputers {
    if (!NSThread.isMainThread) return @"Open diagnostics settings to forget computers.";
    @synchronized(self) {
        self.pairingStorageStatus=RVUSBSavePairings(@{});
        if (self.pairingStorageStatus!=errSecSuccess) return @"Could not remove saved computers from the keychain. Unlock the iPhone and try again.";
        [self.pairings removeAllObjects];
        self.failedPairings=0;
        self.accessCode=[[NSUUID.UUID.UUIDString stringByReplacingOccurrencesOfString:@"-" withString:@""] substringToIndex:8];
        for (NSNumber *client in self.clients) shutdown(client.intValue,SHUT_RDWR);
    }
    return nil;
}
- (void)pause {
    if (!NSThread.isMainThread) { __weak RVUSBDiagnostics *weak=self;dispatch_async(dispatch_get_main_queue(),^{ [weak pause]; });return; }
    BOOL owned=NO;dispatch_source_t source;
    @synchronized(self) {
        if (!self.enabled) return;
        self.enabled=NO;self.generation++;self.accessCode=nil;
        source=self.source;self.source=nil;self.listener=-1;owned=self.ownsCapture;self.ownsCapture=NO;
        self.resumeCapture=owned && RVDAUIDiagnosticsArmed;
        // Workers own close(). shutdown() interrupts their bounded read/write.
        for (NSNumber *client in self.clients) shutdown(client.intValue,SHUT_RDWR);
    }
    if (source) dispatch_source_cancel(source);
    if (owned) RVDAUIStopCapture();
}
- (void)acceptFrom:(int)listener generation:(uint64_t)generation {
    for (NSUInteger attempt=0;attempt<16;attempt++) {
        @synchronized(self) { if (!self.enabled || self.generation!=generation || self.listener!=listener) return; }
        int fd=accept(listener,NULL,NULL);if (fd<0) { if (errno==EINTR) continue;return; }
        BOOL accepted=NO;
        @synchronized(self) {
            accepted=self.enabled && self.generation==generation && self.clients.count<RVUSBClientLimit;
            if (accepted) [self.clients addObject:@(fd)];
        }
        if (!accepted) { close(fd);continue; }
        int yes=1;struct timeval timeout={10,0};
        fcntl(fd,F_SETFL,0);setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
        setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        dispatch_async(self.clientQueue,^{ @autoreleasepool { [self serve:fd generation:generation]; } });
    }
}
- (NSDictionary *)process:(NSDictionary *)request generation:(uint64_t)generation {
    if (![request isKindOfClass:NSDictionary.class]) return RVUSBError(@"invalid_request_schema");
    BOOL debug=[request[@"schema"] isEqual:@2] && [request[@"operation"] isKindOfClass:NSString.class] && [request[@"operation"] hasPrefix:@"debug."];
    if (![request[@"schema"] isEqual:@1] && !debug) return RVUSBError(@"invalid_request_schema");
    NSArray *fields=debug ? @[@"schema",@"operation",@"credential",@"request_id",@"process_nonce",@"arguments"] : @[@"schema",@"operation",@"credential",@"topic",@"client_id",@"client_name"];
    for (NSString *key in request) if (![fields containsObject:key]) return RVUSBError(@"unsupported_request_field");
    NSString *operation=request[@"operation"],*credential=request[@"credential"],*topic=request[@"topic"] ?: @"dearrow";
    if (![operation isKindOfClass:NSString.class] || ![credential isKindOfClass:NSString.class] || credential.length>64 || ![topic isKindOfClass:NSString.class]) return RVUSBError(@"invalid_request_values");
    @synchronized(self) {
        if (!self.enabled || self.generation!=generation) return RVUSBError(@"bridge_paused");
        if ([operation isEqual:@"pair"]) {
            if (self.failedPairings>=8) return RVUSBError(@"pairing_locked_enable_new_session");
            if (![credential.lowercaseString isEqual:self.accessCode.lowercaseString]) {
                return RVUSBError(++self.failedPairings>=8 ? @"pairing_locked_enable_new_session" : @"incorrect_session_code");
            }
            NSString *client=request[@"client_id"],*name=request[@"client_name"] ?: @"Paired computer";
            if (![client isKindOfClass:NSString.class] || client.length!=32 || ![name isKindOfClass:NSString.class] || !name.length || name.length>64) return RVUSBError(@"invalid_pairing_identity");
            if (!self.pairings[client] && self.pairings.count>=8) return RVUSBError(@"saved_computer_limit_forget_computers");
            NSString *token=[NSUUID.UUID.UUIDString stringByReplacingOccurrencesOfString:@"-" withString:@""];
            NSMutableDictionary *updated=[self.pairings mutableCopy];updated[client]=@{@"token":token,@"name":name};
            self.pairingStorageStatus=RVUSBSavePairings(updated);
            if (self.pairingStorageStatus!=errSecSuccess) return RVUSBError(@"pairing_keychain_write_failed");
            self.pairings=updated;self.failedPairings=0;
            return @{@"schema":@1,@"ok":@YES,@"operation":operation,@"token":token,@"data":[self status]};
        }
        BOOL authenticated=NO;
        for (NSDictionary *record in self.pairings.allValues) if ([credential isEqual:record[@"token"]]) { authenticated=YES;break; }
        if (!authenticated) return RVUSBError(@"authentication_required");
    }
    if (debug) return RVDBGProcess(request,credential,^BOOL {
        @synchronized(self) {
            if (!self.enabled || self.generation!=generation) return NO;
            for (NSDictionary *record in self.pairings.allValues) if ([credential isEqual:record[@"token"]]) return YES;
            return NO;
        }
    });
    if ([operation isEqual:@"status"]) return @{@"schema":@1,@"ok":@YES,@"operation":operation,@"data":[self status]};
    if (![@[@"report",@"capture-start",@"capture-stop",@"probe"] containsObject:operation]) return RVUSBError(@"unsupported_operation");
    if ([operation isEqual:@"report"] && ![@[@"all",@"dearrow",@"watch",@"adblock",@"speed",@"ryd",@"sponsorblock",@"navigation",@"adaptive"] containsObject:topic]) return RVUSBError(@"unsupported_report_topic");
    @synchronized(self) { if (self.snapshotPending) return RVUSBError(@"main_queue_snapshot_pending");self.snapshotPending=YES; }
    RVUSBDiagnosticTicket *ticket=[RVUSBDiagnosticTicket new];dispatch_semaphore_t completed=dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_main_queue(),^{
        @try {
            BOOL valid;@synchronized(self) {
                valid=self.enabled && self.generation==generation;
                BOOL trusted=NO;for (NSDictionary *record in self.pairings.allValues) if ([credential isEqual:record[@"token"]]) { trusted=YES;break; }
                valid=valid && trusted;
            }
            if (!valid || ticket.cancelled) ticket.result=RVUSBError(@"bridge_paused");
            else if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) ticket.result=RVUSBError(@"app_not_foreground");
            else {
                NSDictionary *data;
                if ([operation isEqual:@"capture-start"]) { RVDAUIStartCapture();self.ownsCapture=YES;data=@{@"capture_armed":@YES}; }
                else if ([operation isEqual:@"capture-stop"]) { RVDAUIStopCapture();self.ownsCapture=NO;data=@{@"capture_armed":@NO}; }
                else if ([operation isEqual:@"probe"]) {
                    BOOL armed=RVDAUIDiagnosticsArmed,started=RVDAUIProbeCurrentService();
                    if (started && !armed) self.ownsCapture=YES;
                    data=@{@"started":@(started),@"state":started ? @"pending" : @"no_eligible_current_video"};
                } else data=RVDiagnosticsSnapshot(topic);
                ticket.result=data ? @{@"schema":@1,@"ok":@YES,@"operation":operation,@"topic":topic,@"data":data,@"bridge":[self status]} : RVUSBError(@"snapshot_unavailable");
            }
        } @catch (NSException *exception) { ticket.result=RVUSBError(@"snapshot_failed"); }
        @finally { @synchronized(self) { self.snapshotPending=NO; }dispatch_semaphore_signal(completed); }
    });
    if (dispatch_semaphore_wait(completed,dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC))) { ticket.cancelled=YES;return RVUSBError(@"main_queue_snapshot_timeout"); }
    return ticket.result ?: RVUSBError(@"snapshot_unavailable");
}
- (void)serve:(int)fd generation:(uint64_t)generation {
    @try {
        uint32_t size=0;if (!RVUSBRead(fd,&size,sizeof(size))) return;size=ntohl(size);
        if (!size || size>RVUSBRequestLimit) return;
        NSMutableData *data=[NSMutableData dataWithLength:size];if (!RVUSBRead(fd,data.mutableBytes,size)) return;
        id request=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        NSDictionary *response=[self process:request generation:generation];
        NSData *encoded=[NSJSONSerialization dataWithJSONObject:response options:NSJSONWritingSortedKeys error:nil];
        if (!encoded || encoded.length>RVUSBResponseLimit) encoded=[NSJSONSerialization dataWithJSONObject:RVUSBError(@"response_size_limit") options:0 error:nil];
        uint32_t header=htonl((uint32_t)encoded.length);
        if (RVUSBWrite(fd,&header,sizeof(header))) RVUSBWrite(fd,encoded.bytes,encoded.length);
    } @catch (NSException *exception) { /* A malformed client cannot escape the worker. */ }
    @finally { @synchronized(self) { [self.clients removeObject:@(fd)];close(fd); } }
}
@end

static void RVUSBResumeSavedDiagnostics(void) {
    // No listener for users who never opted in, or manually disabled it.
    if (RVCompatible && [NSUserDefaults.standardUserDefaults boolForKey:RVUSBEnabledPreference]) {
        RVDBGSetEnabled([NSUserDefaults.standardUserDefaults boolForKey:RVDBGPreference]);[RVUSBDiagnostics.shared resume];
    }
}
