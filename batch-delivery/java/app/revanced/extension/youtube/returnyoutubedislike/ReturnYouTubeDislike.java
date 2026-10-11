package app.revanced.extension.youtube.returnyoutubedislike;
import static app.revanced.extension.shared.StringRef.str;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.graphics.drawable.shapes.RectShape;
import rv.batch.format.CompactDecimalFormat;
import rv.batch.format.DecimalFormat;
import rv.batch.format.DecimalFormatSymbols;
import rv.batch.format.NumberFormat;
import android.os.Build;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.ReplacementSpan;
import android.widget.Toast;
import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.ui.Dim;
import app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData;
import app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.PlayerType;
// Generated from attributed editable regions, no source-body copying.
public class ReturnYouTubeDislike {
    public static enum Vote {
        LIKE(1),
DISLIKE(-1),
LIKE_REMOVE(0);
        final public int value;
        private Vote(int value) {
            this.value = value;
        }
    }
    final private static long MAX_MILLISECONDS_TO_BLOCK_UI_WAITING_FOR_FETCH = 4000;
    final private static long CACHE_TIMEOUT_SUCCESS_MILLISECONDS = ((7 * 60) * 1000);
    final private static long CACHE_TIMEOUT_FAILURE_MILLISECONDS = ((3 * 60) * 1000);
    final private static char MIDDLE_SEPARATOR_CHARACTER = '\u25ce';
    final private static java.util.Map<java.lang.String,app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike> fetchCache = new java.util.HashMap<>();
    final private static java.util.concurrent.ExecutorService voteSerialExecutor = java.util.concurrent.Executors.newSingleThreadExecutor();
    private static rv.batch.format.CompactDecimalFormat dislikeCountFormatter;
    private static rv.batch.format.NumberFormat dislikePercentageFormatter;
    final public static android.graphics.Rect leftSeparatorBounds;
    final private static android.graphics.Rect middleSeparatorBounds;
    final public static int leftSeparatorShapePaddingPixels;
    final private static android.graphics.drawable.ShapeDrawable leftSeparatorShape;
    static {
        leftSeparatorBounds = new android.graphics.Rect(0, 0, app.revanced.extension.shared.ui.Dim.dp(1.2f), app.revanced.extension.shared.ui.Dim.dp(14.0f));
        final int middleSeparatorSize = app.revanced.extension.shared.ui.Dim.dp(3.7f);
        middleSeparatorBounds = new android.graphics.Rect(0, 0, middleSeparatorSize, middleSeparatorSize);
        leftSeparatorShapePaddingPixels = app.revanced.extension.shared.ui.Dim.dp(8.4f);
        leftSeparatorShape = new android.graphics.drawable.ShapeDrawable(new android.graphics.drawable.shapes.RectShape());
        leftSeparatorShape.setBounds(leftSeparatorBounds);
    }
    final private java.lang.String videoId;
    final private java.util.concurrent.Future<app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData> future;
    final private long timeFetched;
    private boolean isShort;
    private app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote userVote;
    private android.text.Spanned originalDislikeSpan;
    private android.text.SpannableString replacementLikeDislikeSpan;
    private static int getSeparatorColor() {
        return (app.revanced.extension.shared.Utils.isDarkModeEnabled() ? 872415231 : -2500135);
    }
    public static android.graphics.drawable.ShapeDrawable getLeftSeparatorDrawable() {
        leftSeparatorShape.getPaint().setColor(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getSeparatorColor());
        return leftSeparatorShape;
    }
    private static android.text.SpannableString createDislikeSpan(android.text.Spanned oldSpannable, boolean isSegmentedButton, boolean isRollingNumber, app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData voteData) {
        if (((!isSegmentedButton))) {
            return app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpannableWithDislikes(oldSpannable, voteData);
        }
        java.lang.CharSequence oldLikes = oldSpannable;
        if (((!app.revanced.extension.shared.Utils.containsNumber(oldLikes)))) {
            if (((!app.revanced.extension.youtube.settings.Settings.RYD_ESTIMATED_LIKE.get()))) {
                java.lang.String hiddenMessageString = app.revanced.extension.shared.StringRef.str("revanced_ryd_video_likes_hidden_by_video_owner");
                return app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpanUsingStylingOfAnotherSpan(oldSpannable, hiddenMessageString);
            }
            app.revanced.extension.shared.Logger.printDebug(() -> "Using estimated likes");
            oldLikes = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.formatDislikeCount(voteData.getLikeCount());
        }
        android.text.SpannableStringBuilder builder = new android.text.SpannableStringBuilder();
        final boolean compactLayout = app.revanced.extension.youtube.settings.Settings.RYD_COMPACT_LAYOUT.get();
        if (((!compactLayout))) {
            java.lang.String leftSeparatorString = app.revanced.extension.shared.Utils.getTextDirectionString();
            final android.text.Spannable leftSeparatorSpan;
            if ((isRollingNumber)) {
                leftSeparatorSpan = new android.text.SpannableString(leftSeparatorString);
            } else {
                leftSeparatorString += "  ";
                leftSeparatorSpan = new android.text.SpannableString(leftSeparatorString);
                leftSeparatorSpan.setSpan(new app.revanced.extension.youtube.returnyoutubedislike.VerticallyCenteredImageSpan(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getLeftSeparatorDrawable(), false), 1, 2, 17);
                leftSeparatorSpan.setSpan(new app.revanced.extension.youtube.returnyoutubedislike.FixedWidthEmptySpan(leftSeparatorShapePaddingPixels), 2, 3, 17);
            }
            builder.append(leftSeparatorSpan);
        }
        builder.append(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpanUsingStylingOfAnotherSpan(oldSpannable, oldLikes));
        java.lang.String middleSeparatorString = (compactLayout ? (("  " + MIDDLE_SEPARATOR_CHARACTER) + "  ") : ((" \u2009\u2009" + MIDDLE_SEPARATOR_CHARACTER) + "\u2009\u2009 "));
        final int shapeInsertionIndex = (middleSeparatorString.length() / 2);
        android.text.Spannable middleSeparatorSpan = new android.text.SpannableString(middleSeparatorString);
        android.graphics.drawable.ShapeDrawable shapeDrawable = new android.graphics.drawable.ShapeDrawable(new android.graphics.drawable.shapes.OvalShape());
        shapeDrawable.getPaint().setColor(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.getSeparatorColor());
        shapeDrawable.setBounds(middleSeparatorBounds);
        middleSeparatorSpan.setSpan(new app.revanced.extension.youtube.returnyoutubedislike.VerticallyCenteredImageSpan(shapeDrawable, isRollingNumber), shapeInsertionIndex, (shapeInsertionIndex + 1), 17);
        builder.append(middleSeparatorSpan);
        builder.append(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpannableWithDislikes(oldSpannable, voteData));
        return new android.text.SpannableString(builder);
    }
    public static boolean isPreviouslyCreatedSegmentedSpan(java.lang.String text) {
        return (text.indexOf(MIDDLE_SEPARATOR_CHARACTER) >= 0);
    }
    private static boolean spansHaveEqualTextAndColor(android.text.Spanned one, android.text.Spanned two) {
        if (((!one.toString().equals(two.toString())))) {
            return false;
        }
        android.text.style.ForegroundColorSpan[] oneColors = one.getSpans(0, one.length(), android.text.style.ForegroundColorSpan.class);
        android.text.style.ForegroundColorSpan[] twoColors = two.getSpans(0, two.length(), android.text.style.ForegroundColorSpan.class);
        final int oneLength = oneColors.length;
        if (((oneLength != twoColors.length))) {
            return false;
        }
        for (int i = 0; (i < oneLength); i++) {
            if (((oneColors[i].getForegroundColor() != twoColors[i].getForegroundColor()))) {
                return false;
            }
        }
        return true;
    }
    private static android.text.SpannableString newSpannableWithLikes(android.text.Spanned sourceStyling, app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData voteData) {
        return app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpanUsingStylingOfAnotherSpan(sourceStyling, app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.formatDislikeCount(voteData.getLikeCount()));
    }
    private static android.text.SpannableString newSpannableWithDislikes(android.text.Spanned sourceStyling, app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData voteData) {
        return app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpanUsingStylingOfAnotherSpan(sourceStyling, (app.revanced.extension.youtube.settings.Settings.RYD_DISLIKE_PERCENTAGE.get() ? app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.formatDislikePercentage(voteData.getDislikePercentage()) : app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.formatDislikeCount(voteData.getDislikeCount())));
    }
    private static android.text.SpannableString newSpanUsingStylingOfAnotherSpan(android.text.Spanned sourceStyle, java.lang.CharSequence newSpanText) {
        if ((((sourceStyle == newSpanText) && (sourceStyle instanceof android.text.SpannableString)))) {
            return ((android.text.SpannableString)sourceStyle);
        }
        android.text.SpannableString destination = new android.text.SpannableString(newSpanText);
        java.lang.Object[] spans = sourceStyle.getSpans(0, sourceStyle.length(), java.lang.Object.class);
        for (java.lang.Object span : spans) {
            destination.setSpan(span, 0, destination.length(), sourceStyle.getSpanFlags(span));
        }
        return destination;
    }
    private static java.lang.String formatDislikeCount(long dislikeCount) {
        synchronized ((app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.class)) {
            if (((dislikeCountFormatter == null))) {
                java.util.Locale locale = java.util.Locale.getDefault();
                dislikeCountFormatter = rv.batch.format.CompactDecimalFormat.getInstance(locale, rv.batch.format.CompactDecimalFormat.CompactStyle.SHORT);
                if (((android.os.Build.VERSION.SDK_INT >= 28))) {
                    rv.batch.format.DecimalFormatSymbols symbols = rv.batch.format.DecimalFormatSymbols.getInstance(locale);
                    symbols.setDigitStrings(rv.batch.format.DecimalFormatSymbols.getInstance(java.util.Locale.ENGLISH).getDigitStrings());
                    dislikeCountFormatter.setDecimalFormatSymbols(symbols);
                }
            }
            return dislikeCountFormatter.format(dislikeCount);
        }
    }
    private static java.lang.String formatDislikePercentage(float dislikePercentage) {
        java.lang.Object __rv_pattern_input_8 = null;
        rv.batch.format.DecimalFormat __rv_pattern_8 = null;
        synchronized ((app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.class)) {
            if (((dislikePercentageFormatter == null))) {
                java.util.Locale locale = java.util.Locale.getDefault();
                dislikePercentageFormatter = rv.batch.format.NumberFormat.getPercentInstance(locale);
                if ((((android.os.Build.VERSION.SDK_INT >= 28) && (((__rv_pattern_input_8 = dislikePercentageFormatter) instanceof rv.batch.format.DecimalFormat) && ((__rv_pattern_8 = ((rv.batch.format.DecimalFormat)__rv_pattern_input_8)) != null))))) {
                    rv.batch.format.DecimalFormatSymbols symbols = rv.batch.format.DecimalFormatSymbols.getInstance(locale);
                    symbols.setDigitStrings(rv.batch.format.DecimalFormatSymbols.getInstance(java.util.Locale.ENGLISH).getDigitStrings());
                    __rv_pattern_8.setDecimalFormatSymbols(symbols);
                }
            }
            if (((dislikePercentage >= 0.01d))) {
                dislikePercentageFormatter.setMaximumFractionDigits(0);
            } else {
                dislikePercentageFormatter.setMaximumFractionDigits(1);
            }
            return dislikePercentageFormatter.format(dislikePercentage);
        }
    }
    public static app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike getFetchForVideoId(java.lang.String videoId) {
        java.util.Objects.requireNonNull(videoId);
        synchronized ((fetchCache)) {
            final long now = rv.batch.Platform.wallMillis();
            fetchCache.values().removeIf((app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike value) -> {
    final boolean expired = value.isExpired(now);
    if ((expired)) {
        app.revanced.extension.shared.Logger.printDebug(() -> ("Removing expired fetch: " + value.videoId));
    }
    return expired;
});
            app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike fetch = fetchCache.get(videoId);
            if (((fetch == null))) {
                fetch = new app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike(videoId);
                fetchCache.put(videoId, fetch);
            }
            return fetch;
        }
    }
    public static void clearAllUICaches() {
        synchronized ((fetchCache)) {
            for (app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike fetch : fetchCache.values()) {
                fetch.clearUICache();
            }
        }
    }
    private ReturnYouTubeDislike(java.lang.String videoId) {
        this.videoId = java.util.Objects.requireNonNull(videoId);
        this.timeFetched = rv.batch.Platform.wallMillis();
        this.future = app.revanced.extension.shared.Utils.submitOnBackgroundThread(() -> app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.fetchVotes(videoId));
    }
    private boolean isExpired(long now) {
        final long timeSinceCreation = (now - timeFetched);
        if (((timeSinceCreation < CACHE_TIMEOUT_FAILURE_MILLISECONDS))) {
            return false;
        }
        if (((timeSinceCreation > CACHE_TIMEOUT_SUCCESS_MILLISECONDS))) {
            return true;
        }
        return (((!fetchCompleted()) || (getFetchData(MAX_MILLISECONDS_TO_BLOCK_UI_WAITING_FOR_FETCH) == null)));
    }
    public app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData getFetchData(long maxTimeToWait) {
        try {
            return future.get(maxTimeToWait, java.util.concurrent.TimeUnit.MILLISECONDS);
        } catch (java.util.concurrent.TimeoutException ex) {
            app.revanced.extension.shared.Logger.printDebug(() -> (("Waited but future was not complete after: " + maxTimeToWait) + "ms"));
        } catch (final java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "Future failure ", ex);
        }
        return null;
    }
    public boolean fetchCompleted() {
        return future.isDone();
    }
    private synchronized void clearUICache() {
        if (((replacementLikeDislikeSpan != null))) {
            app.revanced.extension.shared.Logger.printDebug(() -> ("Clearing replacement span for: " + videoId));
        }
        replacementLikeDislikeSpan = null;
    }
    public java.lang.String getVideoId() {
        return videoId;
    }
    public synchronized void setVideoIdIsShort(boolean isShort) {
        this.isShort = isShort;
    }
    public synchronized android.text.Spanned getDislikesSpanForRegularVideo(android.text.Spanned original, boolean isSegmentedButton, boolean isRollingNumber) {
        return waitForFetchAndUpdateReplacementSpan(original, isSegmentedButton, isRollingNumber, false, false);
    }
    public synchronized android.text.Spanned getLikeSpanForShort(android.text.Spanned original) {
        return waitForFetchAndUpdateReplacementSpan(original, false, false, true, true);
    }
    public synchronized android.text.Spanned getDislikeSpanForShort(android.text.Spanned original) {
        return waitForFetchAndUpdateReplacementSpan(original, false, false, true, false);
    }
    private android.text.Spanned waitForFetchAndUpdateReplacementSpan(android.text.Spanned original, boolean isSegmentedButton, boolean isRollingNumber, boolean spanIsForShort, boolean spanIsForLikes) {
        try {
            app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData votingData = getFetchData(MAX_MILLISECONDS_TO_BLOCK_UI_WAITING_FOR_FETCH);
            if (((votingData == null))) {
                app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout"), null, null, 0);
                app.revanced.extension.shared.Logger.printDebug(() -> "Cannot add dislike to UI (RYD data not available)");
                return original;
            }
            synchronized ((this)) {
                if ((spanIsForShort)) {
                    isShort = true;
                } else {
                    if ((isShort)) {
                        app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring regular video dislike span, as data loaded was previously used for a Short: " + videoId));
                        return original;
                    }
                }
                if ((spanIsForLikes)) {
                    if (((!app.revanced.extension.shared.Utils.containsNumber(original)))) {
                        if (((!app.revanced.extension.youtube.settings.Settings.RYD_ESTIMATED_LIKE.get()))) {
                            app.revanced.extension.shared.Logger.printDebug(() -> "Likes are hidden");
                            return original;
                        } else {
                            app.revanced.extension.shared.Logger.printDebug(() -> "Using estimated likes");
                        }
                    }
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Creating likes span for: " + votingData.videoId));
                    return app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.newSpannableWithLikes(original, votingData);
                }
                if (((((originalDislikeSpan != null) && (replacementLikeDislikeSpan != null)) && app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.spansHaveEqualTextAndColor(original, originalDislikeSpan)))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ((("Replacing span: " + original) + " with previously created dislike span of data: ") + videoId));
                    return replacementLikeDislikeSpan;
                }
                if (((userVote != null))) {
                    votingData.updateUsingVote(userVote);
                }
                originalDislikeSpan = original;
                replacementLikeDislikeSpan = app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.createDislikeSpan(original, isSegmentedButton, isRollingNumber, votingData);
                app.revanced.extension.shared.Logger.printDebug(() -> ((((("Replaced: '" + originalDislikeSpan) + "' with: '") + replacementLikeDislikeSpan) + "' using video: ") + videoId));
                return replacementLikeDislikeSpan;
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "waitForFetchAndUpdateReplacementSpan failure", ex);
        }
        return original;
    }
    public void sendVote(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote vote) {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        java.util.Objects.requireNonNull(vote);
        try {
            app.revanced.extension.youtube.shared.PlayerType currentType = app.revanced.extension.youtube.shared.PlayerType.getCurrent();
            if (((isShort != currentType.isNoneHiddenOrMinimized()))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ((("Cannot vote for video: " + videoId) + " as current player type does not match: ") + currentType));
                app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_ryd_enabled_while_playing_video_then_user_voted"));
                return;
            }
            setUserVote(vote);
            voteSerialExecutor.execute(() -> {
    try {
        app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.sendVote(videoId, vote);
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "Failed to send vote", ex);
    }
});
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "Error trying to send vote", ex);
        }
    }
    public void setUserVote(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote vote) {
        java.util.Objects.requireNonNull(vote);
        try {
            app.revanced.extension.shared.Logger.printDebug(() -> ("setUserVote: " + vote));
            synchronized ((this)) {
                userVote = vote;
                clearUICache();
            }
            if ((future.isDone())) {
                app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData voteData = getFetchData(MAX_MILLISECONDS_TO_BLOCK_UI_WAITING_FOR_FETCH);
                if (((voteData == null))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> "Cannot update UI (vote data not available)");
                    return;
                }
                voteData.updateUsingVote(vote);
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "setUserVote failure", ex);
        }
    }
}
 class FixedWidthEmptySpan extends android.text.style.ReplacementSpan {
    final int fixedWidth;
     FixedWidthEmptySpan(int fixedWith) {
        this.fixedWidth = fixedWith;
        if (((fixedWith < 0))) {
            throw new java.lang.IllegalArgumentException();
        }
    }
    public int getSize(android.graphics.Paint paint, java.lang.CharSequence text, int start, int end, android.graphics.Paint.FontMetricsInt fontMetrics) {
        return fixedWidth;
    }
    public void draw(android.graphics.Canvas canvas, java.lang.CharSequence text, int start, int end, float x, int top, int y, int bottom, android.graphics.Paint paint) {
    }
}
 class VerticallyCenteredImageSpan extends android.text.style.ImageSpan {
    final boolean useOriginalWidth;
    public VerticallyCenteredImageSpan(android.graphics.drawable.Drawable drawable, boolean useOriginalWidth) {
        super(drawable);
        this.useOriginalWidth = useOriginalWidth;
    }
    public int getSize(android.graphics.Paint paint, java.lang.CharSequence text, int start, int end, android.graphics.Paint.FontMetricsInt fontMetrics) {
        android.graphics.drawable.Drawable drawable = getDrawable();
        android.graphics.Rect bounds = drawable.getBounds();
        if (((fontMetrics != null))) {
            android.graphics.Paint.FontMetricsInt paintMetrics = paint.getFontMetricsInt();
            final int fontHeight = (paintMetrics.descent - paintMetrics.ascent);
            final int drawHeight = (bounds.bottom - bounds.top);
            final int halfDrawHeight = (drawHeight / 2);
            final int yCenter = (paintMetrics.ascent + (fontHeight / 2));
            fontMetrics.ascent = (yCenter - halfDrawHeight);
            fontMetrics.top = fontMetrics.ascent;
            fontMetrics.bottom = (yCenter + halfDrawHeight);
            fontMetrics.descent = fontMetrics.bottom;
        }
        if ((useOriginalWidth)) {
            return ((int)paint.measureText(text, start, end));
        }
        return bounds.right;
    }
    public void draw(android.graphics.Canvas canvas, java.lang.CharSequence text, int start, int end, float x, int top, int y, int bottom, android.graphics.Paint paint) {
        android.graphics.drawable.Drawable drawable = getDrawable();
        canvas.save();
        android.graphics.Paint.FontMetricsInt paintMetrics = paint.getFontMetricsInt();
        final int fontHeight = (paintMetrics.descent - paintMetrics.ascent);
        final int yCenter = ((y + paintMetrics.descent) - (fontHeight / 2));
        final android.graphics.Rect drawBounds = drawable.getBounds();
        float translateX = x;
        if ((useOriginalWidth)) {
            translateX += (((paint.measureText(text, start, end) - ((drawBounds.right - drawBounds.left)))) / 2);
        }
        final int translateY = (yCenter - (((drawBounds.bottom - drawBounds.top)) / 2));
        canvas.translate(translateX, translateY);
        drawable.draw(canvas);
        canvas.restore();
    }
}
