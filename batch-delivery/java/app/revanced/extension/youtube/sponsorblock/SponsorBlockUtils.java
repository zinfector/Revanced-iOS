package app.revanced.extension.youtube.sponsorblock;
import static app.revanced.extension.shared.StringRef.str;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Color;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.widget.EditText;
import java.lang.ref.WeakReference;
import java.text.NumberFormat;
import java.time.Duration;
import java.util.Locale;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour;
import app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory;
import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment;
import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote;
import app.revanced.extension.youtube.sponsorblock.requests.SBRequester;
import app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController;
// Generated from attributed editable regions, no source-body copying.
public class SponsorBlockUtils {
    final private static int LOCKED_COLOR = android.graphics.Color.parseColor("#FFC83D");
    final private static java.lang.String MANUAL_EDIT_TIME_TEXT_HINT = "hh:mm:ss.sss";
    final private static java.util.regex.Pattern manualEditTimePattern = java.util.regex.Pattern.compile("((\\d{1,2}):)?(\\d{1,2}):(\\d{2})(\\.(\\d{1,3}))?");
    final private static java.text.NumberFormat statsNumberFormatter = java.text.NumberFormat.getNumberInstance();
    private static long newSponsorSegmentDialogShownMillis;
    private static long newSponsorSegmentStartMillis = -1;
    private static long newSponsorSegmentEndMillis = -1;
    private static boolean newSponsorSegmentPreviewed;
    final private static android.content.DialogInterface.OnClickListener newSponsorSegmentDialogListener = new android.content.DialogInterface.OnClickListener(){
    public void onClick(android.content.DialogInterface dialog, int which) {
        switch ((which)) {
            case -2:
                newSponsorSegmentStartMillis = newSponsorSegmentDialogShownMillis;
                break;

            case -1:
                newSponsorSegmentEndMillis = newSponsorSegmentDialogShownMillis;
                break;

        }
        dialog.dismiss();
    }
};
    private static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory newUserCreatedSegmentCategory;
    final private static android.content.DialogInterface.OnClickListener segmentTypeListener = new android.content.DialogInterface.OnClickListener(){
    public void onClick(android.content.DialogInterface dialog, int which) {
        try {
            app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutHighlights()[which];
            final boolean enableButton;
            if (((category.behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE))) {
                app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_disabled_category"));
                enableButton = false;
            } else {
                newUserCreatedSegmentCategory = category;
                enableButton = true;
            }
            (((android.app.AlertDialog)dialog)).getButton(-1).setEnabled(enableButton);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "segmentTypeListener failure", ex);
        }
    }
};
    final private static android.content.DialogInterface.OnClickListener segmentReadyDialogButtonListener = new android.content.DialogInterface.OnClickListener(){
    public void onClick(android.content.DialogInterface dialog, int which) {
        try {
            app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.hideNewSegmentLayout();
            android.content.Context context = (((android.app.AlertDialog)dialog)).getContext();
            dialog.dismiss();
            app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categories = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutHighlights();
            java.lang.CharSequence[] titles = new java.lang.CharSequence[categories.length];
            for (int i = 0, length = categories.length; (i < length); i++) {
                titles[i] = categories[i].getTitleWithColorDot();
            }
            newUserCreatedSegmentCategory = null;
            new android.app.AlertDialog.Builder(context).setTitle(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_choose_category")).setSingleChoiceItems(titles, -1, segmentTypeListener).setNegativeButton(17039360, null).setPositiveButton(17039370, segmentCategorySelectedDialogListener).show().getButton(-1).setEnabled(false);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "segmentReadyDialogButtonListener failure", ex);
        }
    }
};
    final private static android.content.DialogInterface.OnClickListener segmentCategorySelectedDialogListener = (android.content.DialogInterface dialog, int which) -> {
    dialog.dismiss();
    app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.submitNewSegment();
};
    final private static app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.EditByHandSaveDialogListener editByHandSaveDialogListener = new app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.EditByHandSaveDialogListener();
    final private static android.content.DialogInterface.OnClickListener editByHandDialogListener = (android.content.DialogInterface dialog, int which) -> {
    try {
        android.content.Context context = (((android.app.AlertDialog)dialog)).getContext();
        final boolean isStart = (-2 == which);
        final android.widget.EditText textView = new android.widget.EditText(context);
        textView.setHint(MANUAL_EDIT_TIME_TEXT_HINT);
        if ((isStart)) {
            if (((newSponsorSegmentStartMillis >= 0))) {
                textView.setText(app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(newSponsorSegmentStartMillis));
            }
        } else {
            if (((newSponsorSegmentEndMillis >= 0))) {
                textView.setText(app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(newSponsorSegmentEndMillis));
            }
        }
        editByHandSaveDialogListener.settingStart = isStart;
        editByHandSaveDialogListener.editTextRef = new java.lang.ref.WeakReference<>(textView);
        new android.app.AlertDialog.Builder(context).setTitle(app.revanced.extension.shared.StringRef.str((isStart ? "revanced_sb_new_segment_time_start" : "revanced_sb_new_segment_time_end"))).setView(textView).setNegativeButton(17039360, null).setNeutralButton(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_now"), editByHandSaveDialogListener).setPositiveButton(17039370, editByHandSaveDialogListener).show();
        dialog.dismiss();
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "editByHandDialogListener failure", ex);
    }
};
    final private static android.content.DialogInterface.OnClickListener segmentVoteClickListener = (android.content.DialogInterface dialog, int which) -> {
    try {
        final android.content.Context context = (((android.app.AlertDialog)dialog)).getContext();
        app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] segments = app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getSegments();
        if ((((segments == null) || (segments.length == 0)))) {
            app.revanced.extension.shared.Logger.printException(() -> "Segment is no longer available on the client");
            return;
        }
        app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment = segments[which];
        app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote[] voteOptions = (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT)) ? app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote.voteTypesWithoutCategoryChange : app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote.values());
        final int voteOptionsLength = voteOptions.length;
        final boolean userIsVip = app.revanced.extension.youtube.settings.Settings.SB_USER_IS_VIP.get();
        java.lang.CharSequence[] items = new java.lang.CharSequence[voteOptionsLength];
        for (int i = 0; (i < voteOptionsLength); i++) {
            app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote voteOption = voteOptions[i];
            java.lang.CharSequence title = voteOption.title.toString();
            if ((((userIsVip && segment.isLocked) && voteOption.highlightIfVipAndVideoIsLocked))) {
                android.text.SpannableString coloredTitle = new android.text.SpannableString(title);
                coloredTitle.setSpan(new android.text.style.ForegroundColorSpan(LOCKED_COLOR), 0, title.length(), 33);
                title = coloredTitle;
            }
            items[i] = title;
        }
        new android.app.AlertDialog.Builder(context).setItems(items, (android.content.DialogInterface dialog1, int which1) -> {
    app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote voteOption = voteOptions[which1];
    switch ((voteOption)) {
        case UPVOTE:

        case DOWNVOTE:
            app.revanced.extension.youtube.sponsorblock.requests.SBRequester.voteForSegmentOnBackgroundThread(segment, voteOption);
            break;

        case CATEGORY_CHANGE:
            app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.onNewCategorySelect(segment, context);
            break;

    }
}).show();
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "segmentVoteClickListener failure", ex);
    }
};
    private SponsorBlockUtils() {
    }
    static void setNewSponsorSegmentPreviewed() {
        newSponsorSegmentPreviewed = true;
    }
    static void clearUnsubmittedSegmentTimes() {
        newSponsorSegmentDialogShownMillis = 0;
        newSponsorSegmentEndMillis = newSponsorSegmentStartMillis = -1;
        newSponsorSegmentPreviewed = false;
    }
    private static void submitNewSegment() {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            final long start = newSponsorSegmentStartMillis;
            final long end = newSponsorSegmentEndMillis;
            final java.lang.String videoId = app.revanced.extension.youtube.patches.VideoInformation.getVideoId();
            final long videoLength = app.revanced.extension.youtube.patches.VideoInformation.getVideoLength();
            final app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory segmentCategory = newUserCreatedSegmentCategory;
            if ((((((((start < 0) || (end < 0)) || (start >= end)) || (videoLength <= 0)) || videoId.isEmpty()) || (segmentCategory == null)))) {
                app.revanced.extension.shared.Logger.printException(() -> "invalid parameters");
                return;
            }
            app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.clearUnsubmittedSegmentTimes();
            app.revanced.extension.shared.Utils.runOnBackgroundThread(() -> {
    try {
        app.revanced.extension.youtube.sponsorblock.requests.SBRequester.submitSegments(videoId, segmentCategory.keyValue, start, end, videoLength);
        app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.executeDownloadSegments(videoId);
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "submitNewSegment failure", ex);
    }
});
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "submitNewSegment failure", ex);
        }
    }
    public static void onMarkLocationClicked() {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            newSponsorSegmentDialogShownMillis = app.revanced.extension.youtube.patches.VideoInformation.getVideoTime();
            new android.app.AlertDialog.Builder(app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.getOverLaysViewGroupContext()).setTitle(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_title")).setMessage(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_time_as_question", app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(newSponsorSegmentDialogShownMillis))).setNeutralButton(17039360, null).setNegativeButton(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_start"), newSponsorSegmentDialogListener).setPositiveButton(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_end"), newSponsorSegmentDialogListener).show();
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onMarkLocationClicked failure", ex);
        }
    }
    public static void onPublishClicked() {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            if ((((newSponsorSegmentStartMillis < 0) || (newSponsorSegmentEndMillis < 0)))) {
                app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_locations_first"));
            } else {
                if (((newSponsorSegmentStartMillis >= newSponsorSegmentEndMillis))) {
                    app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_start_is_before_end"));
                } else {
                    if ((((!newSponsorSegmentPreviewed) && (newSponsorSegmentStartMillis != 0)))) {
                        app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_preview_segment_first"));
                    } else {
                        final long segmentLength = (((newSponsorSegmentEndMillis - newSponsorSegmentStartMillis)) / 1000);
                        new android.app.AlertDialog.Builder(app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.getOverLaysViewGroupContext()).setTitle(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_confirm_title")).setMessage(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_confirm_content", app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(newSponsorSegmentStartMillis), app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(newSponsorSegmentEndMillis), app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.getTimeSavedString(segmentLength))).setNegativeButton(17039369, null).setPositiveButton(17039379, segmentReadyDialogButtonListener).show();
                    }
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onPublishClicked failure", ex);
        }
    }
    public static void onVotingClicked(android.content.Context context) {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] segments = app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.getSegments();
            if ((((segments == null) || (segments.length == 0)))) {
                app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_vote_no_segments"));
                return;
            }
            final int numberOfSegments = segments.length;
            java.lang.CharSequence[] titles = new java.lang.CharSequence[numberOfSegments];
            for (int i = 0; (i < numberOfSegments); i++) {
                app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment = segments[i];
                if (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.UNSUBMITTED))) {
                    continue;
                }
                android.text.SpannableStringBuilder spannableBuilder = new android.text.SpannableStringBuilder();
                spannableBuilder.append(segment.category.getTitleWithColorDot());
                spannableBuilder.append('\n');
                java.lang.String startTime = app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(segment.start);
                if (((segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT))) {
                    spannableBuilder.append(startTime);
                } else {
                    java.lang.String toFromString = app.revanced.extension.shared.StringRef.str("revanced_sb_vote_segment_time_to_from", startTime, app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.formatSegmentTime(segment.end));
                    spannableBuilder.append(toFromString);
                }
                if ((((i + 1) != numberOfSegments))) {
                    spannableBuilder.append('\n');
                }
                spannableBuilder.setSpan(new android.text.style.StyleSpan(1), 0, spannableBuilder.length(), 33);
                titles[i] = spannableBuilder;
            }
            new android.app.AlertDialog.Builder(context).setItems(titles, segmentVoteClickListener).show();
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onVotingClicked failure", ex);
        }
    }
    private static void onNewCategorySelect(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment, android.content.Context context) {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            final app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] values = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutHighlights();
            java.lang.CharSequence[] titles = new java.lang.CharSequence[values.length];
            for (int i = 0; (i < values.length); i++) {
                titles[i] = values[i].getTitleWithColorDot();
            }
            new android.app.AlertDialog.Builder(context).setTitle(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_choose_category")).setItems(titles, (android.content.DialogInterface dialog, int which) -> app.revanced.extension.youtube.sponsorblock.requests.SBRequester.voteToChangeCategoryOnBackgroundThread(segment, values[which])).show();
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onNewCategorySelect failure", ex);
        }
    }
    public static void onPreviewClicked() {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            if ((((newSponsorSegmentStartMillis < 0) || (newSponsorSegmentEndMillis < 0)))) {
                app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_locations_first"));
            } else {
                if (((newSponsorSegmentStartMillis >= newSponsorSegmentEndMillis))) {
                    app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_start_is_before_end"));
                } else {
                    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.removeUnsubmittedSegments();
                    app.revanced.extension.youtube.sponsorblock.SegmentPlaybackController.addUnsubmittedSegment(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.UNSUBMITTED, null, newSponsorSegmentStartMillis, newSponsorSegmentEndMillis, false));
                    app.revanced.extension.youtube.patches.VideoInformation.seekTo((newSponsorSegmentStartMillis - 2000));
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onPreviewClicked failure", ex);
        }
    }
    static void sendViewRequestAsync(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        if (((segment.recordedAsSkipped || (segment.category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.UNSUBMITTED)))) {
            return;
        }
        segment.recordedAsSkipped = true;
        final long totalTimeSkipped = (app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_MILLISECONDS.get() + segment.length());
        app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_MILLISECONDS.save(totalTimeSkipped);
        app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_NUMBER_SEGMENTS.save((app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_NUMBER_SEGMENTS.get() + 1));
        if ((app.revanced.extension.youtube.settings.Settings.SB_TRACK_SKIP_COUNT.get())) {
            app.revanced.extension.shared.Utils.runOnBackgroundThread(() -> app.revanced.extension.youtube.sponsorblock.requests.SBRequester.sendSegmentSkippedViewedRequest(segment));
        }
    }
    public static void showErrorDialog(java.lang.String dialogMessage) {
        app.revanced.extension.shared.Utils.runOnMainThreadNowOrLater(() -> new android.app.AlertDialog.Builder(app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.getOverLaysViewGroupContext()).setMessage(dialogMessage).setPositiveButton(17039370, null).setCancelable(false).show());
    }
    public static void onEditByHandClicked() {
        try {
            app.revanced.extension.shared.Utils.verifyOnMainThread();
            new android.app.AlertDialog.Builder(app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController.getOverLaysViewGroupContext()).setTitle(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_edit_by_hand_title")).setMessage(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_edit_by_hand_content")).setNeutralButton(17039360, null).setNegativeButton(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_start"), editByHandDialogListener).setPositiveButton(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_mark_end"), editByHandDialogListener).show();
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "onEditByHandClicked failure", ex);
        }
    }
    public static java.lang.String getNumberOfSkipsString(int viewCount) {
        return statsNumberFormatter.format(viewCount);
    }
    private static long parseSegmentTime(java.lang.String time) {
        java.util.regex.Matcher matcher = manualEditTimePattern.matcher(time);
        if (((!matcher.matches()))) {
            return -1;
        }
        java.lang.String hoursStr = matcher.group(2);
        java.lang.String minutesStr = matcher.group(3);
        java.lang.String secondsStr = matcher.group(4);
        java.lang.String millisecondsStr = matcher.group(6);
        try {
            final int hours = (((hoursStr != null)) ? java.lang.Integer.parseInt(hoursStr) : 0);
            final int minutes = java.lang.Integer.parseInt(minutesStr);
            final int seconds = java.lang.Integer.parseInt(secondsStr);
            final int milliseconds;
            if (((millisecondsStr != null))) {
                millisecondsStr = java.lang.String.format(java.util.Locale.US, "%-3s", millisecondsStr).replace(' ', '0');
                milliseconds = java.lang.Integer.parseInt(millisecondsStr);
            } else {
                milliseconds = 0;
            }
            return (((((hours * 3600000L)) + ((minutes * 60000L))) + ((seconds * 1000L))) + milliseconds);
        } catch (java.lang.NumberFormatException ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> ("Time format exception: " + time), ex);
            return -1;
        }
    }
    private static java.lang.String formatSegmentTime(long segmentTime) {
        final long videoLength = app.revanced.extension.youtube.patches.VideoInformation.getVideoLength();
        final long hours = java.util.concurrent.TimeUnit.MILLISECONDS.toHours(segmentTime);
        final long minutes = (java.util.concurrent.TimeUnit.MILLISECONDS.toMinutes(segmentTime) % 60);
        final long seconds = (java.util.concurrent.TimeUnit.MILLISECONDS.toSeconds(segmentTime) % 60);
        final long milliseconds = (segmentTime % 1000);
        final java.lang.String formatPattern;
        java.lang.Object[] formatArgs = {minutes, seconds, milliseconds};
        if (((videoLength < (((10 * 60) * 1000))))) {
            formatPattern = "%01d:%02d.%03d";
        } else {
            if (((videoLength < (((60 * 60) * 1000))))) {
                formatPattern = "%02d:%02d.%03d";
            } else {
                if (((videoLength < ((((10 * 60) * 60) * 1000))))) {
                    formatPattern = "%01d:%02d:%02d.%03d";
                    formatArgs = new java.lang.Object[] {hours, minutes, seconds, milliseconds};
                } else {
                    formatPattern = "%02d:%02d:%02d.%03d";
                    formatArgs = new java.lang.Object[] {hours, minutes, seconds, milliseconds};
                }
            }
        }
        return java.lang.String.format(java.util.Locale.US, formatPattern, formatArgs);
    }
    public static java.lang.String getTimeSavedString(long totalSecondsSaved) {
        java.time.Duration duration = java.time.Duration.ofSeconds(totalSecondsSaved);
        final long hours = duration.toHours();
        final long minutes = (duration.toMinutes() % 60);
        java.lang.String minutesFormatted = statsNumberFormatter.format(minutes);
        if (((hours > 0))) {
            java.lang.String hoursFormatted = statsNumberFormatter.format(hours);
            return app.revanced.extension.shared.StringRef.str("revanced_sb_stats_saved_hour_format", hoursFormatted, minutesFormatted);
        }
        final long seconds = (duration.getSeconds() % 60);
        java.lang.String secondsFormatted = statsNumberFormatter.format(seconds);
        if (((minutes > 0))) {
            return app.revanced.extension.shared.StringRef.str("revanced_sb_stats_saved_minute_format", minutesFormatted, secondsFormatted);
        }
        return app.revanced.extension.shared.StringRef.str("revanced_sb_stats_saved_second_format", secondsFormatted);
    }
    private static class EditByHandSaveDialogListener implements android.content.DialogInterface.OnClickListener {
        private boolean settingStart;
        private java.lang.ref.WeakReference<android.widget.EditText> editTextRef = new java.lang.ref.WeakReference<>(null);
        public void onClick(android.content.DialogInterface dialog, int which) {
            try {
                final android.widget.EditText editText = editTextRef.get();
                if (((editText == null))) {
                    return;
                }
                final long time;
                if (((which == -3))) {
                    time = app.revanced.extension.youtube.patches.VideoInformation.getVideoTime();
                } else {
                    time = app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.parseSegmentTime(editText.getText().toString());
                    if (((time < 0))) {
                        app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_new_segment_edit_by_hand_parse_error"));
                        return;
                    }
                }
                if ((settingStart)) {
                    newSponsorSegmentStartMillis = java.lang.Math.max(time, 0);
                } else {
                    newSponsorSegmentEndMillis = time;
                }
                if (((which == -3))) {
                    editByHandDialogListener.onClick(dialog, (settingStart ? -2 : -1));
                }
            } catch (java.lang.Exception ex) {
                app.revanced.extension.shared.Logger.printException(() -> "EditByHandSaveDialogListener failure", ex);
            }
        }
    }
}
