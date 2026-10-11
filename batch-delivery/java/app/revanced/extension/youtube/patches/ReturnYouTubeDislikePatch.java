package app.revanced.extension.youtube.patches;
import static app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote;
import android.graphics.drawable.ShapeDrawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.view.View;
import android.widget.TextView;
import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.Objects;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.youtube.patches.litho.ReturnYouTubeDislikeFilter;
import app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.PlayerType;
// Generated from attributed editable regions, no source-body copying.
public class ReturnYouTubeDislikePatch {
    private static volatile app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike currentVideoData;
    private static volatile app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike lastLithoShortsVideoData;
    private static int useLithoShortsVideoDataCount;
    private static volatile java.lang.String lastPrefetchedVideoId;
    private static void clearData() {
        currentVideoData = null;
        lastLithoShortsVideoData = null;
        synchronized ((app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.class)) {
            useLithoShortsVideoDataCount = 0;
        }
    }
    private static boolean decrementUseLithoDataIfNeeded() {
        synchronized ((app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.class)) {
            if (((useLithoShortsVideoDataCount > 0))) {
                useLithoShortsVideoDataCount--;
                return true;
            }
            return false;
        }
    }
    public static boolean useNewLithoTextCreation(boolean useNewLithoTextCreation) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("useNewLithoTextCreation: " + useNewLithoTextCreation));
        return useNewLithoTextCreation;
    }
    public static java.lang.CharSequence onLithoTextLoaded(java.lang.Object conversionContext, java.lang.CharSequence original) {
        return app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.onLithoTextLoaded(conversionContext, original, false);
    }
    private static java.lang.CharSequence onLithoTextLoaded(java.lang.Object conversionContext, java.lang.CharSequence original, boolean isRollingNumber) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()))) {
                return original;
            }
            java.lang.String conversionContextString = conversionContext.toString();
            if ((app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get())) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("RYD conversion context: " + conversionContext));
            }
            if (((isRollingNumber && (!conversionContextString.contains("video_action_bar.e"))))) {
                return original;
            }
            if ((conversionContextString.contains("segmented_like_dislike_button.e"))) {
                app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike videoData = currentVideoData;
                if (((videoData == null))) {
                    return original;
                }
                if (((!((original instanceof android.text.Spanned))))) {
                    original = new android.text.SpannableString(original);
                }
                return videoData.getDislikesSpanForRegularVideo(((android.text.Spanned)original), true, isRollingNumber);
            }
            if ((isRollingNumber)) {
                return original;
            }
            if ((app.revanced.extension.shared.Utils.containsAny(conversionContextString, "|shorts_dislike_button.e", "|reel_dislike_button.e"))) {
                return app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.getShortsSpan(original, true);
            }
            if ((app.revanced.extension.shared.Utils.containsAny(conversionContextString, "|shorts_like_button.e", "|reel_like_button.e"))) {
                if (((!app.revanced.extension.shared.Utils.containsNumber(original)))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> "Replacing hidden likes count");
                    return app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.getShortsSpan(original, false);
                } else {
                    app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.decrementUseLithoDataIfNeeded();
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onLithoTextLoaded failure", ex);
        }
        return original;
    }
    private static java.lang.CharSequence getShortsSpan(java.lang.CharSequence original, boolean isDislikesSpan) {
        if (((((!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get()) || ((isDislikesSpan && app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_DISLIKE_BUTTON.get()))) || (((!isDislikesSpan) && app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_LIKE_BUTTON.get()))))) {
            return original;
        }
        final app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike videoData;
        if ((app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.decrementUseLithoDataIfNeeded())) {
            videoData = lastLithoShortsVideoData;
        } else {
            videoData = currentVideoData;
        }
        if (((videoData == null))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Cannot modify Shorts litho span, data is null");
            return original;
        }
        return (isDislikesSpan ? videoData.getDislikeSpanForShort(((android.text.Spanned)original)) : videoData.getLikeSpanForShort(((android.text.Spanned)original)));
    }
    private static volatile java.lang.CharSequence rollingNumberSpan;
    public static java.lang.String onRollingNumberLoaded(java.lang.Object conversionContext, java.lang.String original) {
        try {
            java.lang.CharSequence replacement = app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.onLithoTextLoaded(conversionContext, original, true);
            java.lang.String replacementString = replacement.toString();
            if (((!replacementString.equals(original)))) {
                rollingNumberSpan = replacement;
                return replacementString;
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onRollingNumberLoaded failure", ex);
        }
        return original;
    }
    public static float onRollingNumberMeasured(java.lang.String text, float measuredTextWidth) {
        try {
            if ((app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get())) {
                if ((app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.isPreviouslyCreatedSegmentedSpan(text))) {
                    if ((app.revanced.extension.youtube.settings.Settings.RYD_COMPACT_LAYOUT.get())) {
                        return (measuredTextWidth + 1);
                    }
                    return (((measuredTextWidth + 1) + app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.leftSeparatorBounds.right) + app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.leftSeparatorShapePaddingPixels);
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onRollingNumberMeasured failure", ex);
        }
        return measuredTextWidth;
    }
    private static void addRollingNumberPatchChanges(android.widget.TextView view) {
        if (((view.getCompoundDrawablePadding() == 0))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Adding rolling number TextView changes");
            view.setCompoundDrawablePadding(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.leftSeparatorShapePaddingPixels);
            android.graphics.drawable.ShapeDrawable separator = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getLeftSeparatorDrawable();
            if ((app.revanced.extension.shared.Utils.isRightToLeftLocale())) {
                view.setCompoundDrawables(null, null, separator, null);
            } else {
                view.setCompoundDrawables(separator, null, null, null);
            }
            view.setTextAlignment(4);
            view.setSingleLine(true);
        }
    }
    private static void removeRollingNumberPatchChanges(android.widget.TextView view) {
        if (((view.getCompoundDrawablePadding() != 0))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Removing rolling number TextView changes");
            view.setCompoundDrawablePadding(0);
            view.setCompoundDrawables(null, null, null, null);
            view.setTextAlignment(1);
            view.setSingleLine(false);
        }
    }
    public static java.lang.CharSequence updateRollingNumber(android.widget.TextView view, java.lang.CharSequence original) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()))) {
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.removeRollingNumberPatchChanges(view);
                return original;
            }
            if (((!app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.isPreviouslyCreatedSegmentedSpan(original.toString())))) {
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.removeRollingNumberPatchChanges(view);
                return original;
            }
            java.lang.CharSequence replacement = rollingNumberSpan;
            if (((replacement == null))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Cannot update rolling number (field is null");
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.removeRollingNumberPatchChanges(view);
                return original;
            }
            if ((app.revanced.extension.youtube.settings.Settings.RYD_COMPACT_LAYOUT.get())) {
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.removeRollingNumberPatchChanges(view);
            } else {
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.addRollingNumberPatchChanges(view);
            }
            view.setPadding(0, 0, 0, 0);
            return replacement;
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "updateRollingNumber failure", ex);
            return original;
        }
    }
    private static volatile boolean lastPlayerResponseWasShort;
    public static void preloadVideoId(java.lang.String videoId, boolean isShortAndOpeningOrPlaying) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()))) {
                return;
            }
            if ((videoId.equals(lastPrefetchedVideoId))) {
                return;
            }
            if (((!app.revanced.extension.shared.Utils.isNetworkConnected()))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Cannot pre-fetch RYD, network is not connected");
                lastPrefetchedVideoId = null;
                return;
            }
            final boolean videoIdIsShort = app.revanced.extension.youtube.patches.VideoInformation.lastPlayerResponseIsShort();
            if (((videoIdIsShort && (((!isShortAndOpeningOrPlaying) || (!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get())))))) {
                return;
            }
            final boolean waitForFetchToComplete = (videoIdIsShort && (!lastPlayerResponseWasShort));
            app.revanced.extension.shared.Logger.printDebug(() -> ("Prefetching RYD for video: " + videoId));
            app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike fetch = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getFetchForVideoId(videoId);
            if (((waitForFetchToComplete && (!fetch.fetchCompleted())))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Waiting for prefetch to complete: " + videoId));
                fetch.getFetchData(20000);
            }
            lastPlayerResponseWasShort = videoIdIsShort;
            lastPrefetchedVideoId = videoId;
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "preloadVideoId failure", ex);
        }
    }
    public static void newVideoLoaded(java.lang.String videoId) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()))) {
                return;
            }
            java.util.Objects.requireNonNull(videoId);
            app.revanced.extension.youtube.shared.PlayerType currentPlayerType = app.revanced.extension.youtube.shared.PlayerType.getCurrent();
            final boolean isNoneHiddenOrSlidingMinimized = currentPlayerType.isNoneHiddenOrSlidingMinimized();
            if (((isNoneHiddenOrSlidingMinimized && (!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get())))) {
                app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.clearData();
                return;
            }
            if ((app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.videoIdIsSame(currentVideoData, videoId))) {
                return;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ((("New video ID: " + videoId) + " playerType: ") + currentPlayerType));
            if (((!app.revanced.extension.shared.Utils.isNetworkConnected()))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Cannot fetch RYD, network is not connected");
                currentVideoData = null;
                return;
            }
            app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike data = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getFetchForVideoId(videoId);
            if ((isNoneHiddenOrSlidingMinimized)) {
                data.setVideoIdIsShort(true);
            }
            currentVideoData = data;
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "newVideoLoaded failure", ex);
        }
    }
    public static void setLastLithoShortsVideoId(java.lang.String videoId) {
        if ((app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.videoIdIsSame(lastLithoShortsVideoData, videoId))) {
            return;
        }
        if (((videoId == null))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Litho filter did not find any video IDs");
            app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.clearData();
            return;
        }
        app.revanced.extension.shared.Logger.printDebug(() -> ("New litho Shorts video ID: " + videoId));
        app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike videoData = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getFetchForVideoId(videoId);
        videoData.setVideoIdIsShort(true);
        lastLithoShortsVideoData = videoData;
        synchronized ((app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.class)) {
            useLithoShortsVideoDataCount = 2;
        }
    }
    private static boolean videoIdIsSame(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike fetch, java.lang.String videoId) {
        return ((((fetch == null) && (videoId == null))) || (((fetch != null) && fetch.getVideoId().equals(videoId))));
    }
    public static void sendVote(int vote) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()))) {
                return;
            }
            final boolean isNoneHiddenOrMinimized = app.revanced.extension.youtube.shared.PlayerType.getCurrent().isNoneHiddenOrMinimized();
            if (((isNoneHiddenOrMinimized && (!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get())))) {
                return;
            }
            app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike videoData = currentVideoData;
            if (((videoData == null))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Cannot send vote, as current video data is null");
                return;
            }
            for (app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote v : app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote.values()) {
                if (((v.value == vote))) {
                    videoData.sendVote(v);
                    return;
                }
            }
            app.revanced.extension.shared.Logger.printException(() -> ("Unknown vote type: " + vote));
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "sendVote failure", ex);
        }
    }
}
