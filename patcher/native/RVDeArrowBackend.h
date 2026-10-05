// SPDX-License-Identifier: MIT
// Backend only. Native rendering/settings adapters call this interface later.
#pragma once
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN
typedef NS_OPTIONS(NSUInteger, RVDeArrowParts) {
    RVDeArrowPartsNone=0, RVDeArrowPartsTitle=1, RVDeArrowPartsThumbnail=2,
    RVDeArrowPartsAll=RVDeArrowPartsTitle|RVDeArrowPartsThumbnail
};
typedef NS_ENUM(NSUInteger, RVDeArrowDecision) {
    RVDeArrowDecisionDisabled, RVDeArrowDecisionOriginal, RVDeArrowDecisionCustom,
    RVDeArrowDecisionMissing, RVDeArrowDecisionUntrusted, RVDeArrowDecisionInvalid,
    RVDeArrowDecisionUnavailable
};
typedef NS_ENUM(NSUInteger, RVDeArrowImageState) {
    RVDeArrowImageStateNotRequested, RVDeArrowImageStatePending,
    RVDeArrowImageStateReady, RVDeArrowImageStateUnavailable
};
typedef NS_ENUM(NSUInteger, RVDeArrowCheckpoint) {
    RVDeArrowCheckpointBackendReady=1, RVDeArrowCheckpointOwnerBound,
    RVDeArrowCheckpointVideoBound, RVDeArrowCheckpointRoleBound,
    RVDeArrowCheckpointMetadataRequested, RVDeArrowCheckpointMetadataReceived,
    RVDeArrowCheckpointDecisionSelected, RVDeArrowCheckpointImageReceived,
    RVDeArrowCheckpointReplacementApplied, RVDeArrowCheckpointNativeLayout,
    RVDeArrowCheckpointLifecycleRebound, RVDeArrowCheckpointOriginalRestored
};
typedef NS_ENUM(NSUInteger, RVDeArrowBlocker) {
    RVDeArrowBlockerNone, RVDeArrowBlockerInvalidVideoID, RVDeArrowBlockerInvalidOptions,
    RVDeArrowBlockerQueueFull, RVDeArrowBlockerMetadataHTTP, RVDeArrowBlockerMetadataShape,
    RVDeArrowBlockerVideoMissing, RVDeArrowBlockerCandidateUntrusted,
    RVDeArrowBlockerCandidateInvalid, RVDeArrowBlockerCommunityOriginal,
    RVDeArrowBlockerImageHTTP, RVDeArrowBlockerImageNoContent,
    RVDeArrowBlockerImageTimestamp, RVDeArrowBlockerImageDecode,
    RVDeArrowBlockerBodyLimit, RVDeArrowBlockerRedirect, RVDeArrowBlockerNetwork,
    RVDeArrowBlockerBackoff, RVDeArrowBlockerCancelled, RVDeArrowBlockerConfigurationChanged,
    RVDeArrowBlockerOwnerUnknown, RVDeArrowBlockerVideoAmbiguous, RVDeArrowBlockerAdRoot,
    RVDeArrowBlockerTitleRoleUnknown, RVDeArrowBlockerImageRoleUnknown,
    RVDeArrowBlockerStaleGeneration, RVDeArrowBlockerRefreshContract,
    RVDeArrowBlockerNativeTextOverwritten, RVDeArrowBlockerCandidateMissing
};

// Copied/validated by configure:. Endpoints are separate protocols. HTTPS only.
// No settings keys, feature toggles, account state or native views are read here.
@interface RVDeArrowOptions : NSObject <NSCopying>
@property(nonatomic,copy) NSURL *brandingEndpoint;
@property(nonatomic,copy) NSURL *thumbnailEndpoint;
@property(nonatomic) NSTimeInterval metadataTTL; // 60...86400; default 1800
@property(nonatomic) NSTimeInterval negativeTTL; // 10...3600; default 300
@property(nonatomic) NSTimeInterval requestTimeout; // 3...60; default 15
@property(nonatomic) BOOL diagnosticsEnabled;
@property(nonatomic) BOOL persistMetadata; // bounded sanitized cache, default YES
+ (instancetype)defaultOptions;
@end

// Immutable snapshots; a title can be ready while imageState is Pending.
// Custom thumbnail decision means a chosen frame, NOT a successful image fetch.
// Use thumbnailImage only when imageState==Ready. Nil means retain native image.
@interface RVDeArrowResult : NSObject
@property(nonatomic,readonly,copy) NSString *videoID;
@property(nonatomic,readonly) uint64_t configurationRevision;
@property(nonatomic,readonly) uint64_t clientGeneration;
@property(nonatomic,readonly) RVDeArrowDecision titleDecision;
@property(nonatomic,readonly) RVDeArrowDecision thumbnailDecision;
@property(nonatomic,readonly,copy,nullable) NSString *title;
@property(nonatomic,readonly,strong,nullable) NSNumber *thumbnailTimestamp;
@property(nonatomic,readonly,strong,nullable) UIImage *thumbnailImage;
@property(nonatomic,readonly) RVDeArrowImageState imageState;
@property(nonatomic,readonly) RVDeArrowBlocker metadataBlocker;
@property(nonatomic,readonly) RVDeArrowBlocker titleBlocker;
@property(nonatomic,readonly) RVDeArrowBlocker thumbnailSelectionBlocker;
@property(nonatomic,readonly) RVDeArrowBlocker imageBlocker;
@property(nonatomic,readonly) NSInteger metadataHTTPStatus;
@property(nonatomic,readonly) NSInteger imageHTTPStatus;
@property(nonatomic,readonly) BOOL metadataCacheHit;
@property(nonatomic,readonly) BOOL imageCacheHit;
@property(nonatomic,readonly,getter=isFinal) BOOL final;
@end

// Retain in the UI binding; explicitly cancel on reuse/owner teardown.
// Cancel suppresses queued/future completions, but cannot revoke a running block.
// Cancelling one consumer does not cancel another consumer of the same video.
@interface RVDeArrowRequest : NSObject
@property(atomic,readonly,getter=isCancelled) BOOL cancelled;
@property(atomic,readonly) uint64_t configurationRevision;
@property(atomic,readonly) uint64_t clientGeneration;
- (void)cancel;
// Typed, redacted presentation diagnostics; does not change requests/results.
- (void)recordCheckpoint:(RVDeArrowCheckpoint)checkpoint blocker:(RVDeArrowBlocker)blocker;
@end

@interface RVDeArrowBackend : NSObject
+ (instancetype)sharedBackend; // lazy: including this backend performs no I/O
@property(atomic,readonly) uint64_t configurationRevision;
// Thread-safe. Invalid options return NO and leave current options unchanged.
// Successful changed options invalidate tokens; UI must rebind after configuring.
- (BOOL)configure:(RVDeArrowOptions *)options;
// Completion ALWAYS runs asynchronously on main. May deliver metadata then image.
// Result is masked to requested parts; title-only consumers do not wait for images.
// UI must also check its owner/video/clientGeneration before applying a snapshot.
- (RVDeArrowRequest *)requestVideoID:(NSString *)videoID
                              parts:(RVDeArrowParts)parts
                   clientGeneration:(uint64_t)generation
                         completion:(void (^)(RVDeArrowResult *result))completion;
// Invalidate requests and clear memory/disk caches; UI must rebind afterwards.
- (void)clearCaches;
- (NSDictionary *)diagnostics; // bounded hashes/enums/counters; no titles/URLs/IDs
+ (nullable NSString *)cleanSubmittedTitle:(id)value;
@end
NS_ASSUME_NONNULL_END
