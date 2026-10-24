// Run with the queue host command in .github/workflows/build-sponsor-prompts.yml.
// Exercise the shipped matcher and hook installer, rather than reimplementing them.
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <math.h>
#include <stdio.h>

static BOOL RVCompatible=YES;
static void RVLog(NSString *message) { (void)message; }
#include "../native/RVRuntime.inc"

// GPB-style typed extensions and explicit presence selectors. C bool gives the
// production B ABI on both Intel and Apple Silicon Foundation hosts.
@interface GPBMock : NSObject <NSCopying>
@property(nonatomic,strong) NSArray *extensionsCurrentlySet;
@property(nonatomic,strong) NSDictionary *extensionValues;
@property(nonatomic,strong) NSData *data;
@property(nonatomic,strong) NSString *params,*browseId;
@property(nonatomic,strong) NSArray *commandsArray;
@property(nonatomic,strong) id activePlaybackCommand,noActivePlaybackCommand,onTapAction;
@property(nonatomic,strong) id trueCommand,falseCommand,command,navigationEndpoint;
@property(nonatomic,strong) id serviceEndpoint,browseEndpoint;
- (id)getExtension:(id)extension;
- (bool)hasActivePlaybackCommand;
- (bool)hasNoActivePlaybackCommand;
- (bool)hasOnTapAction;
- (bool)hasTrueCommand;
- (bool)hasFalseCommand;
- (bool)hasCommand;
- (bool)hasNavigationEndpoint;
- (bool)hasServiceEndpoint;
- (bool)hasBrowseEndpoint;
@end
@implementation GPBMock
- (id)getExtension:(id)extension { return self.extensionValues[extension]; }
#define PRESENT(Name,field) - (bool)has##Name { return self.field!=nil; }
PRESENT(ActivePlaybackCommand,activePlaybackCommand)
PRESENT(NoActivePlaybackCommand,noActivePlaybackCommand)
PRESENT(OnTapAction,onTapAction)
PRESENT(TrueCommand,trueCommand)
PRESENT(FalseCommand,falseCommand)
PRESENT(Command,command)
PRESENT(NavigationEndpoint,navigationEndpoint)
PRESENT(ServiceEndpoint,serviceEndpoint)
PRESENT(BrowseEndpoint,browseEndpoint)
#undef PRESENT
- (id)copyWithZone:(NSZone *)zone {
    GPBMock *copy=[[[self class] allocWithZone:zone] init];
    copy.extensionsCurrentlySet=self.extensionsCurrentlySet;
    copy.extensionValues=self.extensionValues;copy.data=self.data;
    copy.params=self.params;copy.browseId=self.browseId;copy.commandsArray=self.commandsArray;
    copy.activePlaybackCommand=self.activePlaybackCommand;
    copy.noActivePlaybackCommand=self.noActivePlaybackCommand;copy.onTapAction=self.onTapAction;
    copy.trueCommand=self.trueCommand;copy.falseCommand=self.falseCommand;copy.command=self.command;
    copy.navigationEndpoint=self.navigationEndpoint;copy.serviceEndpoint=self.serviceEndpoint;
    copy.browseEndpoint=self.browseEndpoint;return copy;
}
@end
#define MESSAGE(Name) @interface Name : GPBMock @end @implementation Name @end
MESSAGE(YTICommand)
MESSAGE(YTIGetWatchNextQueueAddCommand)
MESSAGE(YTIQueueAddMenuItemCommand)
MESSAGE(YTIYPCOffersEndpoint)
MESSAGE(YTIYPCGetOfflineUpsellEndpoint)
#undef MESSAGE

@interface MockIcon : NSObject
@property(nonatomic) int iconType;
@end
@implementation MockIcon @end
@interface MockItem : GPBMock
@property(nonatomic,strong) MockIcon *icon;
@property(nonatomic,strong) id secondaryIcon;
@end
@implementation MockItem
- (id)copyWithZone:(NSZone *)zone {
    MockItem *copy=[super copyWithZone:zone];
    copy.icon=self.icon;copy.secondaryIcon=self.secondaryIcon;return copy;
}
@end
@interface YTIMenuItemSupportedRenderers : NSObject <NSCopying>
@property(nonatomic,strong) MockItem *menuConditionalServiceItemRenderer,*menuServiceItemRenderer;
- (bool)hasMenuConditionalServiceItemRenderer;
- (bool)hasMenuServiceItemRenderer;
@end
@implementation YTIMenuItemSupportedRenderers
- (bool)hasMenuConditionalServiceItemRenderer { return self.menuConditionalServiceItemRenderer!=nil; }
- (bool)hasMenuServiceItemRenderer { return self.menuServiceItemRenderer!=nil; }
- (id)copyWithZone:(NSZone *)zone {
    YTIMenuItemSupportedRenderers *copy=[[[self class] allocWithZone:zone] init];
    copy.menuConditionalServiceItemRenderer=self.menuConditionalServiceItemRenderer;
    copy.menuServiceItemRenderer=self.menuServiceItemRenderer;return copy;
}
@end

#include "../native/RVQueue.inc"

static YTICommand *Typed(GPBMock *payload,NSString *identity) {
    YTICommand *command=[YTICommand new];
    command.extensionsCurrentlySet=@[@"typed-extension"];
    command.extensionValues=@{@"typed-extension":payload};
    command.data=[identity dataUsingEncoding:NSUTF8StringEncoding];return command;
}
static YTICommand *Queue(NSString *identity) {
    YTIGetWatchNextQueueAddCommand *payload=[YTIGetWatchNextQueueAddCommand new];
    payload.params=@"opaque-real-queue-params";return Typed(payload,identity);
}
static YTICommand *Offer(void) {
    GPBMock *browse=[GPBMock new];browse.browseId=@"SPunlimited";
    return Typed(browse,@"premium-browse-offer");
}
static YTIMenuItemSupportedRenderers *Item(int icon,YTICommand *endpoint,BOOL conditional) {
    MockItem *item=[MockItem new];item.icon=[MockIcon new];item.icon.iconType=icon;
    item.serviceEndpoint=endpoint;item.secondaryIcon=@"premium-badge";
    YTIMenuItemSupportedRenderers *wrapper=[YTIMenuItemSupportedRenderers new];
    if (conditional) wrapper.menuConditionalServiceItemRenderer=item;
    else wrapper.menuServiceItemRenderer=item;
    return wrapper;
}
static MockItem *Unwrap(YTIMenuItemSupportedRenderers *wrapper) {
    return wrapper.menuConditionalServiceItemRenderer ?: wrapper.menuServiceItemRenderer;
}
static void PairedAndCopyOnWrite(void) {
    YTICommand *next=Queue(@"next-data"),*last=Queue(@"last-data");
    YTIMenuItemSupportedRenderers *nextOffer=Item(251,Offer(),YES);
    YTIMenuItemSupportedRenderers *lastOffer=Item(895,Offer(),NO);
    YTIMenuItemSupportedRenderers *unrelated=Item(99,Offer(),NO);
    NSArray *input=@[nextOffer,Item(251,next,NO),lastOffer,Item(63,last,YES),unrelated];
    NSArray *output=RVQueueMenu(input);
    assert(output!=input && output.count==input.count);
    assert(output[0]!=nextOffer && Unwrap(output[0])!=Unwrap(nextOffer));
    assert(output[2]!=lastOffer && Unwrap(output[2])!=Unwrap(lastOffer));
    assert(Unwrap(output[0]).serviceEndpoint==next);
    assert(Unwrap(output[2]).serviceEndpoint==last);
    assert(Unwrap(output[0]).secondaryIcon==nil && Unwrap(output[2]).secondaryIcon==nil);
    assert(Unwrap(nextOffer).serviceEndpoint!=next && Unwrap(lastOffer).serviceEndpoint!=last);
    assert([Unwrap(nextOffer).secondaryIcon isEqual:@"premium-badge"]);
    assert([Unwrap(lastOffer).secondaryIcon isEqual:@"premium-badge"]);
    assert(output[1]==input[1] && output[3]==input[3] && output[4]==unrelated);
    // A next sibling must never satisfy an isolated last offer.
    NSArray *crossRole=@[lastOffer,Item(251,next,NO)];
    assert(RVQueueMenu(crossRole)==crossRole);
}
static void MissingAmbiguousAndDuplicate(void) {
    YTIMenuItemSupportedRenderers *offer=Item(251,Offer(),NO);
    NSArray *missing=@[offer];assert(RVQueueMenu(missing)==missing);
    NSArray *ambiguous=@[offer,Item(251,Queue(@"first"),NO),Item(251,Queue(@"second"),NO)];
    assert(RVQueueMenu(ambiguous)==ambiguous);
    YTICommand *first=Queue(@"equal-bytes"),*second=Queue(@"equal-bytes");
    assert(first!=second && [first.data isEqual:second.data]);
    NSArray *duplicates=@[offer,Item(251,first,NO),Item(251,second,NO)];
    NSArray *result=RVQueueMenu(duplicates);
    assert(result!=duplicates && [[Unwrap(result[0]).serviceEndpoint data] isEqual:first.data]);
}
static void PresenceAndBounds(void) {
    assert(RVQueueFlags(Typed([YTIYPCOffersEndpoint new],@"typed-offer"))==2);
    YTIGetWatchNextQueueAddCommand *empty=[YTIGetWatchNextQueueAddCommand new];
    assert(RVQueueFlags(Typed(empty,@"absent"))==0);
    empty.params=@"";assert(RVQueueFlags(Typed(empty,@"empty"))==0);
    YTIQueueAddMenuItemCommand *outer=[YTIQueueAddMenuItemCommand new];
    assert(RVQueueFlags(Typed(outer,@"absent-outer-command"))==0);
    outer.noActivePlaybackCommand=[YTICommand new];
    assert(RVQueueFlags(Typed(outer,@"nonqueue-startup-command"))==0);
    outer.noActivePlaybackCommand=nil;
    outer.activePlaybackCommand=Queue(@"nested-active");
    assert(RVQueueFlags(Typed(outer,@"present-active-command"))==1);
    YTICommand *cycle=[YTICommand new];cycle.commandsArray=@[cycle];
    assert(RVQueueFlags(cycle)==0);cycle.commandsArray=nil;
    GPBMock *deep=Queue(@"too-deep");
    for (NSUInteger i=0;i<9;i++) { GPBMock *parent=[GPBMock new];parent.command=deep;deep=parent; }
    assert(RVQueueFlags(deep)==4);
    NSArray *tooDeepMenu=@[Item(251,Offer(),NO),Item(251,Typed(deep,@"deep-candidate"),NO)];
    assert(RVQueueMenu(tooDeepMenu)==tooDeepMenu);
    GPBMock *root=[GPBMock new];NSMutableArray *branches=[NSMutableArray array];
    for (NSUInteger i=0;i<32;i++) {
        GPBMock *branch=[GPBMock new];NSMutableArray *leaves=[NSMutableArray array];
        for (NSUInteger j=0;j<4;j++) [leaves addObject:(i==31 && j==3) ? Queue(@"after-budget") : [GPBMock new]];
        branch.commandsArray=leaves;[branches addObject:branch];
    }
    root.commandsArray=branches;assert(RVQueueFlags(root)==4);
    NSUInteger budget=2;NSHashTable *seen=[NSHashTable hashTableWithOptions:NSPointerFunctionsObjectPointerPersonality];
    assert(RVQueueCommandFlags(root,8,&budget,seen)==4 && budget==0 && seen.count==2);
    YTICommand *oversized=Queue(@"valid-queue-with-unexamined-tail");
    NSMutableArray *tooMany=[NSMutableArray array];
    for (NSUInteger i=0;i<33;i++) [tooMany addObject:[GPBMock new]];
    oversized.commandsArray=tooMany;assert(RVQueueFlags(oversized)==5);
    NSArray *oversizedMenu=@[Item(251,Offer(),NO),Item(251,oversized,NO)];
    assert(RVQueueMenu(oversizedMenu)==oversizedMenu);
    oversized.commandsArray=nil;oversized.extensionsCurrentlySet=tooMany;
    assert(RVQueueFlags(oversized)==4);
    NSArray *menu=@[Item(251,Offer(),NO),Item(251,Queue(@"enabled"),NO)];
    RVCompatible=NO;assert(RVQueueMenu(menu)==menu);RVCompatible=YES;
    RVQueueRewriteDepth=1;assert(RVQueueMenu(menu)==menu);RVQueueRewriteDepth=0;
}
static void OptionalAcquisitionAction(void) {
    YTIGetWatchNextQueueAddCommand *message=[YTIGetWatchNextQueueAddCommand new];
    id offer=Offer(),feedback=Typed([GPBMock new],@"ordinary-feedback");
    assert(RVQueueTapAction(message,offer)==offer);
    message.params=@"real-queue-params";
    assert(RVQueueTapAction(message,offer)==nil);
    assert(RVQueueTapAction(message,feedback)==feedback);
    id queue=Queue(@"queued-native-command");
    assert(RVQueueTapAction(message,queue)==queue);
    YTICommand *mixed=[YTICommand new];mixed.commandsArray=@[queue,offer];
    assert(RVQueueFlags(mixed)==3 && RVQueueTapAction(message,mixed)==mixed);
    assert(RVQueueTapAction(message,nil)==nil);
}

@interface HookParent : NSObject
- (id)probe;
@end
@implementation HookParent
- (id)probe { return @"original"; }
@end
@interface HookChild : HookParent @end
@implementation HookChild @end
static void InheritedHook(void) {
    SEL sel=@selector(probe);
    IMP parent=class_getMethodImplementation(HookParent.class,sel);
    assert(!RVHook(@"HookChild",@"probe",@"v@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) { return @"wrong-signature"; };
    }));
    assert(class_getMethodImplementation(HookChild.class,sel)==parent);
    assert(RVHook(@"HookChild",@"probe",@"@@:",NO,^id(IMP original,SEL selector) {
        return ^id(id object) {
            return [((id(*)(id,SEL))original)(object,selector) stringByAppendingString:@"-hooked"];
        };
    }));
    assert([[HookChild.new probe] isEqual:@"original-hooked"]);
    assert([[HookParent.new probe] isEqual:@"original"]);
    assert(class_getMethodImplementation(HookParent.class,sel)==parent);
}
int main(void) {
    @autoreleasepool {
        assert(RVCan([GPBMock new],"hasCommand",@"B@:"));
        PairedAndCopyOnWrite();MissingAmbiguousAndDuplicate();PresenceAndBounds();OptionalAcquisitionAction();InheritedHook();
        assert(atomic_load(&RVQueueRewrites)>=3 && atomic_load(&RVQueueFailures)==0);
        NSDictionary *report=@{@"ok":@YES,@"suite":@"queue-upsell-foundation-host",
            @"test_groups":@5,@"rewrites":@(atomic_load(&RVQueueRewrites)),
            @"native_failures":@(atomic_load(&RVQueueFailures)),@"device_validation":@NO};
        NSData *json=[NSJSONSerialization dataWithJSONObject:report options:0 error:NULL];
        assert(json!=nil);puts([[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding].UTF8String);
    }
    return 0;
}
