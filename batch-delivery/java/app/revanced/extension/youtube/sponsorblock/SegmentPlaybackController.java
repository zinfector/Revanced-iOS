package app.revanced.extension.youtube.sponsorblock;
import static app.revanced.extension.shared.StringRef.str;
import static app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.SKIP_AUTOMATICALLY;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.text.TextUtils;
import android.util.Range;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.Animation;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.annotation.Nullable;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.ui.Dim;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.PlayerControlsVisibility;
import app.revanced.extension.youtube.shared.PlayerType;
import app.revanced.extension.youtube.shared.ShortsPlayerState;
import app.revanced.extension.youtube.shared.VideoState;
import app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour;
import app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory;
import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment;
import app.revanced.extension.youtube.sponsorblock.requests.SBRequester;
import app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController;
import kotlin.Unit;
// Generated from attributed editable regions, no source-body copying.
public class SegmentPlaybackController {
    public static enum SponsorBlockDuration {
        ONE_SECOND(1),
TWO_SECONDS(2),
THREE_SECONDS(3),
FOUR_SECONDS(4),
FIVE_SECONDS(5),
SIX_SECONDS(6),
SEVEN_SECONDS(7),
EIGHT_SECONDS(8),
NINE_SECONDS(9),
TEN_SECONDS(10);
        final private long adjustedDuration;
        private SponsorBlockDuration(int seconds) {
            adjustedDuration = ((seconds * 1000L) - 200);
        }
    }
    final private static int HIGHLIGHT_SEGMENT_DRAW_BAR_WIDTH = 7;
    private static java.lang.String currentVideoId;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] segments;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment highlightSegment;
    private static long highlightSegmentInitialShowEndTime;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segmentCurrentlyPlaying;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment scheduledHideSegment;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment scheduledUpcomingSegment;
    final private static java.util.List<app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment> hiddenSkipSegmentsForCurrentVideoTime = new java.util.ArrayList<>();
    private static android.util.Range<java.lang.Long> undoAutoSkipRange;
    private static android.util.Range<java.lang.Long> undoAutoSkipRangeToast;
    private static long skipSegmentButtonEndTime;
    private static java.lang.String timeWithoutSegments;
    private static int seekbarAbsoluteLeft;
    private static int seekbarAbsoluteRight;
    private static int seekbarThickness;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment lastSegmentSkipped;
    private static long lastSegmentSkippedTime;
    private static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment toastSegmentSkipped;
    private static int toastNumberOfSegmentsSkipped;
    private static java.lang.ref.WeakReference<android.app.Dialog> toastDialogRef = new java.lang.ref.WeakReference<>(null);
    private static volatile int adProgressTextVisibility = -1;
    static {
        app.revanced.extension.youtube.shared.PlayerType.getOnChange().addObserver((app.revanced.extension.youtube.shared.PlayerType type) -> {
    if ((((type == app.revanced.extension.youtube.shared.PlayerType.WATCH_WHILE_PICTURE_IN_PICTURE) && app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.dismissUndoToast()))) {
        app.revanced.extension.shared.Logger.printDebug(() -> "Dismissed undo toast as playback is PiP");
    }
    return kotlin.Unit.INSTANCE;
});
    }
    private static boolean dismissUndoToast() {
        android.app.Dialog toastDialog = toastDialogRef.get();
        if ((((toastDialog != null) && toastDialog.isShowing()))) {
            toastDialog.dismiss();
            return true;
        }
        return false;
    }
    private static long getSkipButtonDuration() {
        return app.revanced.extension.youtube.settings.Settings.SB_AUTO_HIDE_SKIP_BUTTON_DURATION.get().adjustedDuration;
    }
    private static long getToastDuration() {
        return app.revanced.extension.youtube.settings.Settings.SB_TOAST_ON_SKIP_DURATION.get().adjustedDuration;
    }
    static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] getSegments() {
        return segments;
    }
    private static void setSegments(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] videoSegments) {
        java.util.Arrays.sort(videoSegments);
        segments = videoSegments;
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.calculateTimeWithoutSegments();
        if ((((app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT.behaviour == SKIP_AUTOMATICALLY) || (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT.behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.MANUAL_SKIP)))) {
            for (app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment : videoSegments) {
                if (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT))) {
                    highlightSegment = segment;
                    return;
                }
            }
        }
        highlightSegment = null;
    }
    static void addUnsubmittedSegment(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        java.util.Objects.requireNonNull(segment);
        if (((segments == null))) {
            segments = new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[1];
        } else {
            segments = java.util.Arrays.copyOf(segments, (segments.length + 1));
        }
        segments[(segments.length - 1)] = segment;
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegments(segments);
    }
    static void removeUnsubmittedSegments() {
        if ((((segments == null) || (segments.length == 0)))) {
            return;
        }
        java.util.List<app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment> replacement = new java.util.ArrayList<>();
        for (app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment : segments) {
            if (((segment.category != app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.UNSUBMITTED))) {
                replacement.add(segment);
            }
        }
        if (((replacement.size() != segments.length))) {
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegments(replacement.toArray(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[0]));
        }
    }
    public static boolean videoHasSegments() {
        return ((segments != null) && (segments.length > 0));
    }
    private static void clearData() {
        currentVideoId = null;
        segments = null;
        highlightSegment = null;
        highlightSegmentInitialShowEndTime = 0;
        timeWithoutSegments = null;
        segmentCurrentlyPlaying = null;
        scheduledUpcomingSegment = null;
        scheduledHideSegment = null;
        skipSegmentButtonEndTime = 0;
        toastSegmentSkipped = null;
        toastNumberOfSegmentsSkipped = 0;
        undoAutoSkipRange = null;
        undoAutoSkipRangeToast = null;
        hiddenSkipSegmentsForCurrentVideoTime.clear();
    }
    public static void initialize(app.revanced.extension.youtube.patches.VideoInformation.PlaybackController ignoredPlayerController) {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.initialize();
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.clearData();
            app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideAll();
            app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.clearUnsubmittedSegmentTimes();
            app.revanced.extension.shared.Logger.printDebug(() -> "Initialized SponsorBlock");
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "initialize failure", ex);
        }
    }
    public static void setCurrentVideoId(java.lang.String videoId) {
        try {
            if ((java.util.Objects.equals(currentVideoId, videoId))) {
                return;
            }
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.clearData();
            if ((((videoId == null) || (!app.revanced.extension.youtube.settings.Settings.SB_ENABLED.get())))) {
                return;
            }
            if ((app.revanced.extension.youtube.shared.ShortsPlayerState.isOpen())) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Ignoring Short");
                return;
            }
            if (((!app.revanced.extension.shared.Utils.isNetworkConnected()))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Network not connected, ignoring video");
                return;
            }
            currentVideoId = videoId;
            app.revanced.extension.shared.Logger.printDebug(() -> ("New video ID: " + videoId));
            app.revanced.extension.shared.Utils.runOnBackgroundThread(() -> {
    try {
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.executeDownloadSegments(videoId);
    } catch (java.lang.Exception e) {
        app.revanced.extension.shared.Logger.printException(() -> "Failed to download segments", e);
    }
});
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "setCurrentVideoId failure", ex);
        }
    }
    static void executeDownloadSegments(java.lang.String videoId) {
        java.util.Objects.requireNonNull(videoId);
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] segments = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getSegments(videoId);
        app.revanced.extension.shared.Utils.runOnMainThread(() -> {
    if (((!videoId.equals(currentVideoId)))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring segments for prior video: " + videoId));
        return;
    }
    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegments(segments);
    final long videoTime = app.revanced.extension.youtube.patches.VideoInformation.getVideoTime();
    if (((highlightSegment != null))) {
        final long timeUntilHighlight = (highlightSegment.start - videoTime);
        if (((timeUntilHighlight > 0))) {
            if ((highlightSegment.shouldAutoSkip())) {
                app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.skipSegment(highlightSegment, false);
                return;
            }
            highlightSegmentInitialShowEndTime = (rv.batch.Platform.wallMillis() + java.lang.Math.min(((long)((timeUntilHighlight / app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed()))), app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getSkipButtonDuration()));
        }
    }
    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setVideoTime(videoTime);
});
    }
    public static void setAdProgressTextVisibility(int visibility) {
        if (((adProgressTextVisibility != visibility))) {
            adProgressTextVisibility = visibility;
            app.revanced.extension.shared.Logger.printDebug(() -> {
    java.lang.String visibilityMessage = switch ((visibility)) {    case 0 -> "VISIBLE";
    case 8 -> "GONE";
    case 4 -> "INVISIBLE";
    default -> "UNKNOWN";};
    return ("AdProgressText visibility changed to: " + visibilityMessage);
});
        }
    }
    public static boolean isAdProgressTextVisible() {
        return (adProgressTextVisibility == 0);
    }
    private static boolean autoSkipIsEnabledAndPlayerOverlayIsActive() {
        return (app.revanced.extension.youtube.settings.Settings.SB_AUTO_HIDE_SKIP_BUTTON.get() && (app.revanced.extension.youtube.shared.PlayerControlsVisibility.getCurrent() != app.revanced.extension.youtube.shared.PlayerControlsVisibility.PLAYER_CONTROLS_VISIBILITY_HIDDEN));
    }
    public static void setVideoTime(long millis) {
        try {
            if (((((((!app.revanced.extension.youtube.settings.Settings.SB_ENABLED.get()) || app.revanced.extension.youtube.shared.PlayerType.getCurrent().isNoneOrHidden()) || (segments == null)) || (segments.length == 0)) || app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.isAdProgressTextVisible()))) {
                return;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ("setVideoTime: " + millis));
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.updateHiddenSegments(millis);
            final float playbackSpeed = app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed();
            final long speedAdjustedTimeThreshold = ((long)((playbackSpeed * 1200)));
            final long startTimerLookAheadThreshold = (millis + speedAdjustedTimeThreshold);
            app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment foundSegmentCurrentlyPlaying = null;
            app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment foundUpcomingSegment = null;
            for (final app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment : segments) {
                if (((((segment.category.behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.SHOW_IN_SEEKBAR) || (segment.category.behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE)) || (segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT)))) {
                    continue;
                }
                if (((segment.end <= millis))) {
                    continue;
                }
                final boolean segmentShouldAutoSkip = app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.shouldAutoSkipAndUndoSkipNotActive(segment, millis);
                if (((segment.start <= millis))) {
                    if ((segmentShouldAutoSkip)) {
                        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.skipSegment(segment, false);
                        return;
                    }
                    if ((((foundSegmentCurrentlyPlaying == null) || foundSegmentCurrentlyPlaying.containsSegment(segment)))) {
                        final long minMillisOfSegmentRemainingThreshold = 800;
                        if ((((segmentCurrentlyPlaying == segment) || (!segment.endIsNear(millis, minMillisOfSegmentRemainingThreshold))))) {
                            foundSegmentCurrentlyPlaying = segment;
                        } else {
                            app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring segment that ends very soon: " + segment));
                        }
                    }
                    continue;
                }
                if (((startTimerLookAheadThreshold < segment.start))) {
                    break;
                }
                if ((segmentShouldAutoSkip)) {
                    foundUpcomingSegment = segment;
                    break;
                }
                if ((((((foundSegmentCurrentlyPlaying == null) || foundSegmentCurrentlyPlaying.containsSegment(segment))) && (((foundUpcomingSegment == null) || foundUpcomingSegment.containsSegment(segment)))))) {
                    final long minTimeBetweenStartEndOfSegments = 1000;
                    if ((((foundSegmentCurrentlyPlaying == null) || (!foundSegmentCurrentlyPlaying.endIsNear(segment.start, minTimeBetweenStartEndOfSegments))))) {
                        foundUpcomingSegment = segment;
                    } else {
                        app.revanced.extension.shared.Logger.printDebug(() -> ("Not scheduling segment (start time is near end of current segment): " + segment));
                    }
                }
            }
            if (((highlightSegment != null))) {
                if ((((millis < app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getSkipButtonDuration()) || (((highlightSegmentInitialShowEndTime != 0) && (rv.batch.Platform.wallMillis() < highlightSegmentInitialShowEndTime)))))) {
                    app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.showSkipHighlightButton(highlightSegment);
                } else {
                    highlightSegmentInitialShowEndTime = 0;
                    app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipHighlightButton();
                }
            }
            if (((segmentCurrentlyPlaying != foundSegmentCurrentlyPlaying))) {
                app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegmentCurrentlyPlaying(foundSegmentCurrentlyPlaying);
            } else {
                if (((((foundSegmentCurrentlyPlaying != null) && (skipSegmentButtonEndTime != 0)) && (skipSegmentButtonEndTime <= rv.batch.Platform.wallMillis())))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Auto hiding skip button for segment: " + segmentCurrentlyPlaying));
                    skipSegmentButtonEndTime = 0;
                    hiddenSkipSegmentsForCurrentVideoTime.add(foundSegmentCurrentlyPlaying);
                    if (((!app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.autoSkipIsEnabledAndPlayerOverlayIsActive()))) {
                        app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipSegmentButton();
                    }
                }
            }
            final app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segmentToHide = ((((foundSegmentCurrentlyPlaying != null) && foundSegmentCurrentlyPlaying.endIsNear(millis, speedAdjustedTimeThreshold))) ? foundSegmentCurrentlyPlaying : null);
            if (((scheduledHideSegment != segmentToHide))) {
                if (((segmentToHide == null))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Clearing scheduled hide: " + scheduledHideSegment));
                    scheduledHideSegment = null;
                } else {
                    scheduledHideSegment = segmentToHide;
                    app.revanced.extension.shared.Logger.printDebug(() -> ((("Scheduling hide segment: " + segmentToHide) + " playbackSpeed: ") + playbackSpeed));
                    final long delayUntilHide = ((long)((((segmentToHide.end - millis)) / playbackSpeed)));
                    app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    if (((scheduledHideSegment != segmentToHide))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring old scheduled hide segment: " + segmentToHide));
        return;
    }
    scheduledHideSegment = null;
    if (((app.revanced.extension.youtube.shared.VideoState.getCurrent() != app.revanced.extension.youtube.shared.VideoState.PLAYING))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring scheduled hide segment as video is paused: " + segmentToHide));
        return;
    }
    final long videoTime = app.revanced.extension.youtube.patches.VideoInformation.getVideoTime();
    if (((!segmentToHide.endIsNear(videoTime, speedAdjustedTimeThreshold)))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ((("Ignoring outdated scheduled hide: " + segmentToHide) + " videoInformation time: ") + videoTime));
        return;
    }
    app.revanced.extension.shared.Logger.printDebug(() -> ("Running scheduled hide segment: " + segmentToHide));
    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegmentCurrentlyPlaying(null);
    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setVideoTime(segmentToHide.end);
}, delayUntilHide);
                }
            }
            if (((scheduledUpcomingSegment != foundUpcomingSegment))) {
                if (((foundUpcomingSegment == null))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Clearing scheduled segment: " + scheduledUpcomingSegment));
                    scheduledUpcomingSegment = null;
                } else {
                    scheduledUpcomingSegment = foundUpcomingSegment;
                    final app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segmentToSkip = foundUpcomingSegment;
                    app.revanced.extension.shared.Logger.printDebug(() -> ((("Scheduling segment: " + segmentToSkip) + " playbackSpeed: ") + playbackSpeed));
                    final long delayUntilSkip = ((long)((((segmentToSkip.start - millis)) / playbackSpeed)));
                    app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    if (((scheduledUpcomingSegment != segmentToSkip))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring old scheduled segment: " + segmentToSkip));
        return;
    }
    scheduledUpcomingSegment = null;
    if (((app.revanced.extension.youtube.shared.VideoState.getCurrent() != app.revanced.extension.youtube.shared.VideoState.PLAYING))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring scheduled hide segment as video is paused: " + segmentToSkip));
        return;
    }
    final long videoTime = app.revanced.extension.youtube.patches.VideoInformation.getVideoTime();
    if (((!segmentToSkip.startIsNear(videoTime, speedAdjustedTimeThreshold)))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ((("Ignoring outdated scheduled segment: " + segmentToSkip) + " videoInformation time: ") + videoTime));
        return;
    }
    if ((app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.shouldAutoSkipAndUndoSkipNotActive(segmentToSkip, videoTime))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Running scheduled skip segment: " + segmentToSkip));
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.skipSegment(segmentToSkip, false);
    } else {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Running scheduled show segment: " + segmentToSkip));
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegmentCurrentlyPlaying(segmentToSkip);
    }
}, delayUntilSkip);
                }
                if ((((undoAutoSkipRange != null) && (!undoAutoSkipRange.contains(millis))))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Clearing undo range as current time is now outside range: " + undoAutoSkipRange));
                    undoAutoSkipRange = null;
                }
            }
        } catch (java.lang.Exception e) {
            app.revanced.extension.shared.Logger.printException(() -> "setVideoTime failure", e);
        }
    }
    private static void updateHiddenSegments(long currentVideoTime) {
        hiddenSkipSegmentsForCurrentVideoTime.removeIf((app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment hiddenSegment) -> {
    if (((!hiddenSegment.containsTime(currentVideoTime)))) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Resetting hide skip button: " + hiddenSegment));
        return true;
    }
    return false;
});
    }
    private static void setSegmentCurrentlyPlaying(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        if (((segment == null))) {
            if (((segmentCurrentlyPlaying != null))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Hiding segment: " + segmentCurrentlyPlaying));
            }
            segmentCurrentlyPlaying = null;
            skipSegmentButtonEndTime = 0;
            app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipSegmentButton();
            return;
        }
        segmentCurrentlyPlaying = segment;
        skipSegmentButtonEndTime = 0;
        if ((app.revanced.extension.youtube.settings.Settings.SB_AUTO_HIDE_SKIP_BUTTON.get())) {
            if ((hiddenSkipSegmentsForCurrentVideoTime.contains(segment))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring previously auto-hidden segment: " + segment));
                app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.setSkipSegment(segment);
                if (((!app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.autoSkipIsEnabledAndPlayerOverlayIsActive()))) {
                    app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipSegmentButton();
                }
                return;
            }
            skipSegmentButtonEndTime = (rv.batch.Platform.wallMillis() + app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getSkipButtonDuration());
        }
        app.revanced.extension.shared.Logger.printDebug(() -> ("Showing segment: " + segment));
        app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.showSkipSegmentButton(segment);
    }
    private static void skipSegment(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segmentToSkip, boolean userManuallySkipped) {
        try {
            app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipHighlightButton();
            app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipSegmentButton();
            final long now = rv.batch.Platform.wallMillis();
            if (((lastSegmentSkipped == segmentToSkip))) {
                final long minTimeBetweenSkippingSameSegment = java.lang.Math.max(500, ((long)((500 / app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed()))));
                if ((((now - lastSegmentSkippedTime) < minTimeBetweenSkippingSameSegment))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring skip segment request (already skipped as close as possible): " + segmentToSkip));
                    return;
                }
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ((("Skipping segment: " + segmentToSkip) + " videoState: ") + app.revanced.extension.youtube.shared.VideoState.getCurrent()));
            lastSegmentSkipped = segmentToSkip;
            lastSegmentSkippedTime = now;
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.setSegmentCurrentlyPlaying(null);
            scheduledHideSegment = null;
            scheduledUpcomingSegment = null;
            if (((segmentToSkip == highlightSegment))) {
                highlightSegmentInitialShowEndTime = 0;
            }
            android.util.Range<java.lang.Long> range = segmentToSkip.getUndoRange();
            if (((undoAutoSkipRange == null))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Setting new undo range to: " + range));
                undoAutoSkipRange = range;
            } else {
                android.util.Range<java.lang.Long> extendedRange = undoAutoSkipRange.extend(range);
                app.revanced.extension.shared.Logger.printDebug(() -> ((("Extending undo range from: " + undoAutoSkipRange) + " to: ") + extendedRange));
                undoAutoSkipRange = extendedRange;
            }
            undoAutoSkipRangeToast = undoAutoSkipRange;
            final boolean seekSuccessful = app.revanced.extension.youtube.patches.VideoInformation.seekTo(segmentToSkip.end);
            if (((!seekSuccessful))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Could not skip segment (seek unsuccessful): " + segmentToSkip));
                return;
            }
            if (((!userManuallySkipped))) {
                final boolean showSkipToast = app.revanced.extension.youtube.settings.Settings.SB_TOAST_ON_SKIP.get();
                for (app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment otherSegment : java.util.Objects.requireNonNull(segments)) {
                    if (((otherSegment.end <= segmentToSkip.start))) {
                        continue;
                    }
                    if (((segmentToSkip.end <= otherSegment.start))) {
                        break;
                    }
                    if ((((otherSegment == segmentToSkip) || (((otherSegment.category != app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT) && segmentToSkip.containsSegment(otherSegment)))))) {
                        otherSegment.didAutoSkipped = true;
                        if ((showSkipToast)) {
                            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.showSkippedSegmentToast(otherSegment);
                        }
                    }
                }
            }
            if (((segmentToSkip.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.UNSUBMITTED))) {
                app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.removeUnsubmittedSegments();
                app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.setNewSponsorSegmentPreviewed();
            } else {
                if (((app.revanced.extension.youtube.shared.VideoState.getCurrent() != app.revanced.extension.youtube.shared.VideoState.PAUSED))) {
                    app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.sendViewRequestAsync(segmentToSkip);
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "skipSegment failure", ex);
        }
    }
    private static boolean shouldAutoSkipAndUndoSkipNotActive(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment, long currentVideoTime) {
        return (segment.shouldAutoSkip() && (((undoAutoSkipRange == null) || (!undoAutoSkipRange.contains(currentVideoTime)))));
    }
    public static boolean currentlyInsideSkippableSegment() {
        return ((segmentCurrentlyPlaying != null) || (!hiddenSkipSegmentsForCurrentVideoTime.isEmpty()));
    }
    public static boolean shouldNotFadeOutPlayerOverlaySkipButton() {
        return ((skipSegmentButtonEndTime != 0) || (!app.revanced.extension.youtube.settings.Settings.SB_AUTO_HIDE_SKIP_BUTTON.get()));
    }
    private static void showSkippedSegmentToast(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        toastSegmentSkipped = segment;
        if (((toastNumberOfSegmentsSkipped++ > 0))) {
            return;
        }
        final long delayToToastMilliseconds = 250;
        app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    try {
        if (((app.revanced.extension.youtube.shared.VideoState.getCurrent() == app.revanced.extension.youtube.shared.VideoState.PAUSED))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Ignoring scheduled toast as video state is paused");
            return;
        }
        if (((app.revanced.extension.youtube.shared.PlayerType.getCurrent() == app.revanced.extension.youtube.shared.PlayerType.WATCH_WHILE_PICTURE_IN_PICTURE))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Not showing autoskip toast as playback is PiP");
            return;
        }
        if ((((toastSegmentSkipped == null) || (undoAutoSkipRangeToast == null)))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Ignoring old scheduled show toast");
            return;
        }
        java.lang.String message = ((toastNumberOfSegmentsSkipped == 1) ? toastSegmentSkipped.getSkippedToastText() : app.revanced.extension.shared.StringRef.str("revanced_sb_skipped_multiple_segments"));
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.showAutoSkipToast(message, undoAutoSkipRangeToast);
    } finally {
        toastNumberOfSegmentsSkipped = 0;
        toastSegmentSkipped = null;
    }
}, delayToToastMilliseconds);
    }
    private static void showAutoSkipToast(java.lang.String messageToToast, android.util.Range<java.lang.Long> rangeToUndo) {
        java.util.Objects.requireNonNull(messageToToast);
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        if (((app.revanced.extension.youtube.shared.PlayerType.getCurrent() == app.revanced.extension.youtube.shared.PlayerType.INLINE_MINIMAL))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Not showing undo toast for feed playback");
            return;
        }
        android.content.Context currentContext = app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.getOverLaysViewGroupContext();
        if (((currentContext == null))) {
            app.revanced.extension.shared.Logger.printException(() -> ("Cannot show toast (context is null): " + messageToToast));
            return;
        }
        app.revanced.extension.shared.Logger.printDebug(() -> ("Showing toast: " + messageToToast));
        android.app.Dialog dialog = new android.app.Dialog(currentContext);
        dialog.requestWindowFeature(1);
        dialog.setCanceledOnTouchOutside(false);
        android.widget.LinearLayout mainLayout = new android.widget.LinearLayout(currentContext);
        mainLayout.setOrientation(1);
        mainLayout.setPadding(16, 8, 16, 8);
        mainLayout.setGravity(17);
        mainLayout.setMinimumHeight(48);
        android.graphics.drawable.ShapeDrawable background = new android.graphics.drawable.ShapeDrawable(new android.graphics.drawable.shapes.RoundRectShape(app.revanced.extension.shared.ui.Dim.roundedCorners(20), null, null));
        background.getPaint().setColor(app.revanced.extension.shared.Utils.getDialogBackgroundColor());
        mainLayout.setBackground(background);
        android.widget.TextView textView = new android.widget.TextView(currentContext);
        textView.setText(messageToToast);
        textView.setTextSize(14);
        textView.setTextColor(app.revanced.extension.shared.Utils.getAppForegroundColor());
        textView.setGravity(17);
        android.widget.LinearLayout.LayoutParams textParams = new android.widget.LinearLayout.LayoutParams(-2, -2);
        textParams.gravity = 17;
        textView.setLayoutParams(textParams);
        mainLayout.addView(textView);
        mainLayout.setAlpha(0.8f);
        final int fadeDurationFast = app.revanced.extension.shared.Utils.getResourceInteger("fade_duration_fast");
        android.view.animation.Animation fadeIn = app.revanced.extension.shared.Utils.getResourceAnimation("fade_in");
        android.view.animation.Animation fadeOut = app.revanced.extension.shared.Utils.getResourceAnimation("fade_out");
        fadeIn.setDuration(fadeDurationFast);
        fadeOut.setDuration(fadeDurationFast);
        fadeOut.setAnimationListener(new android.view.animation.Animation.AnimationListener(){
    public void onAnimationStart(android.view.animation.Animation animation) {
    }
    public void onAnimationEnd(android.view.animation.Animation animation) {
        if ((dialog.isShowing())) {
            dialog.dismiss();
        }
    }
    public void onAnimationRepeat(android.view.animation.Animation animation) {
    }
});
        mainLayout.setOnClickListener((android.view.View v) -> {
    try {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Undoing autoskip using range: " + rangeToUndo));
        undoAutoSkipRange = rangeToUndo;
        app.revanced.extension.youtube.patches.VideoInformation.seekTo(rangeToUndo.getLower());
        mainLayout.startAnimation(fadeOut);
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "showToastShortWithTapAction setOnClickListener failure", ex);
        dialog.dismiss();
    }
});
        mainLayout.setClickable(true);
        dialog.setContentView(mainLayout);
        android.view.Window window = dialog.getWindow();
        if (((window != null))) {
            window.setWindowAnimations(0);
            window.addFlags(32);
            window.addFlags(262144);
            app.revanced.extension.shared.Utils.setDialogWindowParameters(window, 80, 72, 60, true);
        }
        if ((app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.dismissUndoToast())) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Dismissed previous skip toast that was still on screen");
        }
        toastDialogRef = new java.lang.ref.WeakReference<>(dialog);
        mainLayout.startAnimation(fadeIn);
        dialog.show();
        app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    if ((dialog.isShowing())) {
        mainLayout.startAnimation(fadeOut);
    }
}, app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getToastDuration());
    }
    public static void onSkipSegmentClicked(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        try {
            if (((((segment != highlightSegment) && (segment != segmentCurrentlyPlaying)) && (!hiddenSkipSegmentsForCurrentVideoTime.contains(segment))))) {
                app.revanced.extension.shared.Logger.printException(() -> "error: segment not available to skip");
                app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipSegmentButton();
                app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideSkipHighlightButton();
                return;
            }
            app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.skipSegment(segment, true);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onSkipSegmentClicked failure", ex);
        }
    }
    public static void setSeekbarRectangle(android.graphics.Rect seekbarRect) {
        final int left = seekbarRect.left;
        final int right = seekbarRect.right;
        if ((((seekbarAbsoluteLeft != left) || (seekbarAbsoluteRight != right)))) {
            app.revanced.extension.shared.Logger.printDebug(() -> ((("setSeekbarRectangle left: " + left) + " right: ") + right));
            seekbarAbsoluteLeft = left;
            seekbarAbsoluteRight = right;
        }
    }
    public static void setSeekbarThickness(int thickness) {
        seekbarThickness = thickness;
    }
    public static java.lang.String appendTimeWithoutSegments(java.lang.String totalTime) {
        try {
            if (((((app.revanced.extension.youtube.settings.Settings.SB_ENABLED.get() && app.revanced.extension.youtube.settings.Settings.SB_VIDEO_LENGTH_WITHOUT_SEGMENTS.get()) && (!android.text.TextUtils.isEmpty(totalTime))) && (!android.text.TextUtils.isEmpty(timeWithoutSegments))))) {
                return (("\u202d" + totalTime) + timeWithoutSegments);
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "appendTimeWithoutSegments failure", ex);
        }
        return totalTime;
    }
    private static void calculateTimeWithoutSegments() {
        final long currentVideoLength = app.revanced.extension.youtube.patches.VideoInformation.getVideoLength();
        if ((((((!app.revanced.extension.youtube.settings.Settings.SB_VIDEO_LENGTH_WITHOUT_SEGMENTS.get()) || (currentVideoLength <= 0)) || (segments == null)) || (segments.length == 0)))) {
            timeWithoutSegments = null;
            return;
        }
        boolean foundNonhighlightSegments = false;
        long timeWithoutSegmentsValue = currentVideoLength;
        for (int i = 0, length = segments.length; (i < length); i++) {
            app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment = segments[i];
            if (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT))) {
                continue;
            }
            foundNonhighlightSegments = true;
            long start = segment.start;
            final long end = segment.end;
            for (int j = 0; (j < i); j++) {
                start = java.lang.Math.max(start, segments[j].end);
            }
            if (((start < end))) {
                timeWithoutSegmentsValue -= ((end - start));
            }
        }
        if (((!foundNonhighlightSegments))) {
            timeWithoutSegments = null;
            return;
        }
        final long hours = (timeWithoutSegmentsValue / 3600000);
        final long minutes = (((timeWithoutSegmentsValue / 60000)) % 60);
        final long seconds = (((timeWithoutSegmentsValue / 1000)) % 60);
        if (((hours > 0))) {
            timeWithoutSegments = java.lang.String.format(java.util.Locale.ENGLISH, "\u2009(%d:%02d:%02d)", hours, minutes, seconds);
        } else {
            timeWithoutSegments = java.lang.String.format(java.util.Locale.ENGLISH, "\u2009(%d:%02d)", minutes, seconds);
        }
    }
    public static void drawSegmentTimeBars(final android.graphics.Canvas canvas, final float posY) {
        try {
            if ((((segments == null) || app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.isAdProgressTextVisible()))) {
                return;
            }
            final long videoLength = app.revanced.extension.youtube.patches.VideoInformation.getVideoLength();
            if (((videoLength <= 0))) {
                return;
            }
            final int thicknessDiv2 = (seekbarThickness / 2);
            final float top = (posY - ((seekbarThickness - thicknessDiv2)));
            final float bottom = (posY + thicknessDiv2);
            final float videoMillisecondsToPixels = (((1.0f / videoLength)) * ((seekbarAbsoluteRight - seekbarAbsoluteLeft)));
            final float leftPadding = seekbarAbsoluteLeft;
            for (app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment : segments) {
                final float left = (leftPadding + (segment.start * videoMillisecondsToPixels));
                final float right;
                if (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT))) {
                    right = (left + HIGHLIGHT_SEGMENT_DRAW_BAR_WIDTH);
                } else {
                    right = (leftPadding + (segment.end * videoMillisecondsToPixels));
                }
                canvas.drawRect(left, top, right, bottom, segment.category.paint);
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "drawSponsorTimeBars failure", ex);
        }
    }
}
