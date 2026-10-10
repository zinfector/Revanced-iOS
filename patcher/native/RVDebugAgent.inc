// SPDX-License-Identifier: MIT
#include "RVDebugState.inc"
#include "RVDebugMemory.inc"
#include "RVDebugObjects.inc"
#include "RVDebugRuntime.inc"
static id RVDBGAttachEntitlement(void) {
    typedef CFTypeRef (*Create)(CFAllocatorRef);typedef CFTypeRef (*Copy)(CFTypeRef,CFStringRef,CFErrorRef *);
    Create create=(Create)dlsym(RTLD_DEFAULT,"SecTaskCreateFromSelf");Copy copy=(Copy)dlsym(RTLD_DEFAULT,"SecTaskCopyValueForEntitlement");
    if (!create || !copy) return NSNull.null;CFTypeRef task=create(kCFAllocatorDefault);if (!task) return NSNull.null;
    CFTypeRef value=copy(task,CFSTR("get-task-allow"),NULL);CFRelease(task);id result=NSNull.null;
    if (value) { if (CFGetTypeID(value)==CFBooleanGetTypeID()) result=@(CFBooleanGetValue(value));CFRelease(value); }return result;
}
static NSArray *RVDBGOperations(void) {
    return @[@"debug.images",@"debug.regions",@"debug.read",@"debug.write",@"debug.rollback",@"debug.allocate",@"debug.free",@"debug.object",@"debug.get",@"debug.items",@"debug.value",@"debug.invoke",@"debug.retain",@"debug.release",@"debug.functions",@"debug.call",@"debug.dearrow.bindings",@"debug.native.graph",@"debug.native.roles",@"debug.dearrow.refresh",@"debug.events",@"debug.runtime.bindings",@"debug.runtime.field",@"debug.runtime.setting",@"debug.runtime.visibility",@"debug.runtime.report"];
}
static NSDictionary *RVDBGStatus(void) {
    RVDBGState *state=RVDBGState.shared;@synchronized(state) {
        return @{@"schema":@2,@"enabled":@(RVDBGEnabled()),@"process_nonce":state.nonce,@"pid":@(getpid()),@"bundle_id":NSBundle.mainBundle.bundleIdentifier ?: @"",
            @"ios_version":UIDevice.currentDevice.systemVersion,@"architecture":@"arm64",@"patcher_version":RV_PORT_VERSION,@"source_sha256":RV_BUILD_SOURCE_SHA256,
            @"get_task_allow":RVDBGAttachEntitlement(),@"operations":RVDBGOperations(),@"read_limit":@65536,@"write_limit":@1024,@"handles_limit":@1024,@"jobs_limit":@64,
            @"native_calls":@"compiled_fixed_prototype_adapters",@"thread_control":@"external_lldb_only",@"handles":@(state.handles.count),@"jobs":@(state.jobs.count),@"mutation_budget_remaining":@(2048-state.mutations.count),@"event_sequence":@(state.sequence),
            @"recent_events":RVDBGEnabled() ? [state.events subarrayWithRange:NSMakeRange(state.events.count>24 ? state.events.count-24 : 0,MIN((NSUInteger)24,state.events.count))] : @[],
            @"recent_events_require_main_queue":@NO};
    }
}
static NSDictionary *RVDBGExecute(NSString *operation,NSDictionary *args,NSString *owner) {
    if ([@[@"debug.images",@"debug.regions",@"debug.read",@"debug.write",@"debug.rollback",@"debug.allocate",@"debug.free"] containsObject:operation]) return RVDBGMemory(operation,args,owner);
    if ([@[@"debug.object",@"debug.get",@"debug.items",@"debug.value",@"debug.invoke",@"debug.retain",@"debug.release"] containsObject:operation]) return RVDBGObjectOperation(operation,args);
    if ([@[@"debug.runtime.bindings",@"debug.runtime.field",@"debug.runtime.setting",@"debug.runtime.visibility",@"debug.runtime.report"] containsObject:operation]) return RVDBGRuntimeOperation(operation,args);
    if ([operation isEqual:@"debug.functions"]) return @{@"ok":@YES,@"functions":RVDBGFunctions()};
    if ([operation isEqual:@"debug.call"]) return RVDBGCall(args);
    if ([@[@"debug.dearrow.bindings",@"debug.native.graph",@"debug.native.roles",@"debug.dearrow.refresh"] containsObject:operation]) return RVDBGDeArrow(operation,args);
    if ([operation isEqual:@"debug.events"]) {
        uint64_t after=0;if (args[@"after"] && !RVDBGAddress(args[@"after"],&after)) return RVDBGFail(@"event_cursor_invalid");
        RVDBGState *state=RVDBGState.shared;@synchronized(state) {
            NSMutableArray *events=[NSMutableArray array];for (NSDictionary *event in state.events) if ([event[@"sequence"] unsignedLongLongValue]>after) [events addObject:event];
            return @{@"ok":@YES,@"events":events,@"last_sequence":RVDBGHex(state.sequence),@"dropped_total":@(state.dropped)};
        }
    }return RVDBGFail(@"unsupported_debug_operation");
}
static NSDictionary *RVDBGJobResult(RVDBGJob *job) {
    return @{@"job_id":job.identifier,@"operation":job.operation,@"state":job.state,@"queued_at":@(job.queuedAt),@"completed_at":@(job.completedAt),@"result":job.result ?: @{},@"process_nonce":RVDBGState.shared.nonce};
}
static NSDictionary *RVDBGProcess(NSDictionary *request,NSString *owner,BOOL (^authorized)(void)) {
    NSString *operation=request[@"operation"],*identifier=request[@"request_id"];NSDictionary *args=request[@"arguments"] ?: @{};
    if (![operation isKindOfClass:NSString.class] || ![identifier isKindOfClass:NSString.class] || ![[NSUUID alloc] initWithUUIDString:identifier] || ![args isKindOfClass:NSDictionary.class]) return @{@"schema":@2,@"ok":@NO,@"error":@"invalid_debug_request"};
    RVDBGState *state=RVDBGState.shared;NSDictionary *(^reply)(NSDictionary *,BOOL)=^NSDictionary *(NSDictionary *data,BOOL ok) {
        return @{@"schema":@2,@"ok":@(ok),@"request_id":identifier,@"operation":operation,@"process_nonce":state.nonce,@"source_sha256":RV_BUILD_SOURCE_SHA256,@"data":data};
    };
    if ([operation isEqual:@"debug.status"]) return reply(RVDBGStatus(),YES);
    if (!RVDBGEnabled()) return reply(RVDBGFail(@"enable_live_debugging_in_app"),NO);
    if (![request[@"process_nonce"] isEqual:state.nonce]) return reply(RVDBGFail(@"process_restarted_refresh_capabilities"),NO);
    if ([operation isEqual:@"debug.job"] || [operation isEqual:@"debug.cancel"]) {
        if (![args[@"job_id"] isKindOfClass:NSString.class]) return reply(RVDBGFail(@"job_id_required"),NO);
        @synchronized(state) {
            RVDBGJob *job=state.jobs[args[@"job_id"]];if (!job || ![job.owner isEqual:owner]) return reply(RVDBGFail([state.mutations containsObject:args[@"job_id"]] ? @"job_result_expired_will_not_repeat" : @"job_not_found"),NO);
            if ([operation isEqual:@"debug.cancel"]) {
                if (![job.state isEqual:@"queued"]) return reply(RVDBGFail(@"job_already_started_cannot_cancel"),NO);
                job.state=@"cancelled";job.result=RVDBGFail(@"cancelled_before_execution");job.completedAt=NSProcessInfo.processInfo.systemUptime;
            }return reply(RVDBGJobResult(job),YES);
        }
    }
    if (![RVDBGOperations() containsObject:operation]) return reply(RVDBGFail(@"unsupported_debug_operation"),NO);
    @synchronized(state) {
        if (![request[@"process_nonce"] isEqual:state.nonce]) return reply(RVDBGFail(@"process_restarted_refresh_capabilities"),NO);
        RVDBGJob *prior=state.jobs[identifier];
        if (prior) { if (![prior.owner isEqual:owner] || ![prior.operation isEqual:operation] || ![prior.arguments isEqual:args]) return reply(RVDBGFail(@"request_id_conflict"),NO);return reply(RVDBGJobResult(prior),YES); }
        NSUInteger pending=0;for (RVDBGJob *job in state.jobs.allValues) pending+=[@[@"queued",@"running"] containsObject:job.state];if (pending>=8) return reply(RVDBGFail(@"pending_job_limit"),NO);
        // Read jobs can be retried safely; mutation IDs remain tombstones even
        // after their results leave the bounded ledger. Reusing them never calls
        // native code again. Turning debug mode off resets the process nonce.
        if ([state.mutations containsObject:identifier]) return reply(RVDBGFail(@"job_result_expired_will_not_repeat"),NO);
        BOOL mutation=[@[@"debug.write",@"debug.rollback",@"debug.allocate",@"debug.free",@"debug.invoke",@"debug.call",@"debug.retain",@"debug.release",@"debug.native.roles",@"debug.dearrow.refresh",@"debug.runtime.setting",@"debug.runtime.visibility"] containsObject:operation];
        if (mutation && state.mutations.count>=2048) return reply(RVDBGFail(@"mutation_budget_reset_debug_mode"),NO);
        if (state.jobs.count>=64) for (NSString *key in state.jobOrder.copy) {
            RVDBGJob *old=state.jobs[key];if (![@[@"queued",@"running"] containsObject:old.state]) { [state.jobs removeObjectForKey:key];[state.jobOrder removeObject:key];break; }
        }
        if (mutation) [state.mutations addObject:identifier];
        RVDBGJob *job=[RVDBGJob new];job.identifier=identifier;job.owner=owner;job.operation=operation;job.arguments=args;job.state=@"queued";job.nonce=state.nonce;job.queuedAt=NSProcessInfo.processInfo.systemUptime;
        state.jobs[identifier]=job;[state.jobOrder addObject:identifier];
        dispatch_async(dispatch_get_main_queue(),^{
            @synchronized(state) { if (![job.state isEqual:@"queued"]) return;job.state=@"running"; }
            NSDictionary *result;
            @try {
                if (!RVDBGEnabled() || ![job.nonce isEqual:state.nonce] || !authorized()) result=RVDBGFail(@"debug_authorization_revoked");
                else if (UIApplication.sharedApplication.applicationState!=UIApplicationStateActive) result=RVDBGFail(@"app_not_foreground");
                else result=RVDBGExecute(operation,args,owner);
            } @catch (NSException *exception) { result=RVDBGFail(@"native_operation_exception"); }
            @synchronized(state) { job.result=result ?: RVDBGFail(@"native_operation_no_result");job.state=[result[@"ok"] boolValue] ? @"completed" : @"failed";job.completedAt=NSProcessInfo.processInfo.systemUptime; }
            if (![operation isEqual:@"debug.events"]) RVDBGRecord(@"debug_job_finished",0,@"debug",nil);
        });return reply(RVDBGJobResult(job),YES);
    }
}
