// SPDX-License-Identifier: MIT
// YouTube 21.39.4 client queue presentation. This is not server entitlement.
static void RVInstallQueue(void) {
    // Install during RVStart, before watch/playlist controllers cache this flag.
    // The iOS phase-4 getter covers both the playlist controller's three-way
    // OR and the iOS/V2 checks used by Watch Next and miniplayer code.
    RVHook(@"YTColdConfig",@"iosEnableQueuePhase4",@"B@:",NO,^id(IMP original,SEL sel) {
        return ^BOOL(id object) { return YES; };
    });
}
