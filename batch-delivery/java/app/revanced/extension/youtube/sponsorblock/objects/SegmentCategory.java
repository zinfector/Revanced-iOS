package app.revanced.extension.youtube.sponsorblock.objects;
import static app.revanced.extension.shared.StringRef.sf;
import static app.revanced.extension.youtube.settings.Settings.*;
import android.graphics.Color;
import android.graphics.Paint;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.StringRef;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.settings.StringSetting;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public enum SegmentCategory {
    SPONSOR("sponsor", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_sponsor"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_sponsor_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_sponsor"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_sponsor"), SB_CATEGORY_SPONSOR, SB_CATEGORY_SPONSOR_COLOR),
SELF_PROMO("selfpromo", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_selfpromo"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_selfpromo_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_selfpromo"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_selfpromo"), SB_CATEGORY_SELF_PROMO, SB_CATEGORY_SELF_PROMO_COLOR),
INTERACTION("interaction", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_interaction"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_interaction_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_interaction"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_interaction"), SB_CATEGORY_INTERACTION, SB_CATEGORY_INTERACTION_COLOR),
HIGHLIGHT("poi_highlight", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_highlight"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_highlight_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_highlight"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_highlight"), SB_CATEGORY_HIGHLIGHT, SB_CATEGORY_HIGHLIGHT_COLOR),
INTRO("intro", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_intro"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_intro_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_intro_beginning"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_intro_middle"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_intro_end"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_intro_beginning"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_intro_middle"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_intro_end"), SB_CATEGORY_INTRO, SB_CATEGORY_INTRO_COLOR),
OUTRO("outro", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_outro"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_outro_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_outro"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_outro"), SB_CATEGORY_OUTRO, SB_CATEGORY_OUTRO_COLOR),
PREVIEW("preview", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_preview"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_preview_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_preview_beginning"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_preview_middle"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_preview_end"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_preview_beginning"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_preview_middle"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_preview_end"), SB_CATEGORY_PREVIEW, SB_CATEGORY_PREVIEW_COLOR),
HOOK("hook", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_hook"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_hook_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_hook"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_hook"), SB_CATEGORY_HOOK, SB_CATEGORY_HOOK_COLOR),
FILLER("filler", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_filler"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_filler_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_filler"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_filler"), SB_CATEGORY_FILLER, SB_CATEGORY_FILLER_COLOR),
MUSIC_OFFTOPIC("music_offtopic", app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_nomusic"), app.revanced.extension.shared.StringRef.sf("revanced_sb_segments_nomusic_sum"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_nomusic"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_nomusic"), SB_CATEGORY_MUSIC_OFFTOPIC, SB_CATEGORY_MUSIC_OFFTOPIC_COLOR),
UNSUBMITTED("unsubmitted", app.revanced.extension.shared.StringRef.empty, app.revanced.extension.shared.StringRef.empty, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_unsubmitted"), app.revanced.extension.shared.StringRef.sf("revanced_sb_skipped_unsubmitted"), SB_CATEGORY_UNSUBMITTED, SB_CATEGORY_UNSUBMITTED_COLOR);
    final private static app.revanced.extension.shared.StringRef skipSponsorTextCompact = app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_compact");
    final private static app.revanced.extension.shared.StringRef skipSponsorTextCompactHighlight = app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_button_compact_highlight");
    final private static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categoriesWithoutHighlights = new app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] {SPONSOR, SELF_PROMO, INTERACTION, INTRO, OUTRO, PREVIEW, HOOK, FILLER, MUSIC_OFFTOPIC};
    final private static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categoriesWithoutUnsubmitted = new app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] {SPONSOR, SELF_PROMO, INTERACTION, HIGHLIGHT, INTRO, OUTRO, PREVIEW, HOOK, FILLER, MUSIC_OFFTOPIC};
    final public static java.lang.String COLOR_DOT_STRING = "\u2b24";
    final public static float CATEGORY_DEFAULT_OPACITY = 0.7f;
    final private static java.util.Map<java.lang.String,app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory> mValuesMap = new java.util.HashMap<>((2 * categoriesWithoutUnsubmitted.length));
    public static java.lang.String sponsorBlockAPIFetchCategories = "[]";
    static {
        for (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory value : categoriesWithoutUnsubmitted) {
            mValuesMap.put(value.keyValue, value);
        }
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categoriesWithoutUnsubmitted() {
        return categoriesWithoutUnsubmitted;
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categoriesWithoutHighlights() {
        return categoriesWithoutHighlights;
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory byCategoryKey(java.lang.String key) {
        return mValuesMap.get(key);
    }
    public static void updateEnabledCategories() {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        app.revanced.extension.shared.Logger.printDebug(() -> "updateEnabledCategories");
        app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categories = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutUnsubmitted();
        java.util.List<java.lang.String> enabledCategories = new java.util.ArrayList<>(categories.length);
        for (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category : categories) {
            if (((category.behaviour != app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE))) {
                enabledCategories.add(category.keyValue);
            }
        }
        if ((enabledCategories.isEmpty())) {
            sponsorBlockAPIFetchCategories = "[]";
        } else {
            sponsorBlockAPIFetchCategories = (("[%22" + android.text.TextUtils.join("%22,%22", enabledCategories)) + "%22]");
        }
    }
    public static void loadAllCategoriesFromSettings() {
        for (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category : app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.values()) {
            category.loadFromSettings();
        }
        app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.updateEnabledCategories();
    }
    final public java.lang.String keyValue;
    final public app.revanced.extension.shared.settings.StringSetting behaviorSetting;
    final public app.revanced.extension.shared.settings.StringSetting colorSetting;
    final public app.revanced.extension.shared.StringRef title;
    final public app.revanced.extension.shared.StringRef description;
    final public app.revanced.extension.shared.StringRef skipButtonTextBeginning;
    final public app.revanced.extension.shared.StringRef skipButtonTextMiddle;
    final public app.revanced.extension.shared.StringRef skipButtonTextEnd;
    final public app.revanced.extension.shared.StringRef skippedToastBeginning;
    final public app.revanced.extension.shared.StringRef skippedToastMiddle;
    final public app.revanced.extension.shared.StringRef skippedToastEnd;
    final public android.graphics.Paint paint;
    private int color;
    public app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour = app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE;
    private SegmentCategory(java.lang.String keyValue, app.revanced.extension.shared.StringRef title, app.revanced.extension.shared.StringRef description, app.revanced.extension.shared.StringRef skipButtonText, app.revanced.extension.shared.StringRef skippedToastText, app.revanced.extension.shared.settings.StringSetting behavior, app.revanced.extension.shared.settings.StringSetting color) {
        this(keyValue, title, description, skipButtonText, skipButtonText, skipButtonText, skippedToastText, skippedToastText, skippedToastText, behavior, color);
    }
    private SegmentCategory(java.lang.String keyValue, app.revanced.extension.shared.StringRef title, app.revanced.extension.shared.StringRef description, app.revanced.extension.shared.StringRef skipButtonTextBeginning, app.revanced.extension.shared.StringRef skipButtonTextMiddle, app.revanced.extension.shared.StringRef skipButtonTextEnd, app.revanced.extension.shared.StringRef skippedToastBeginning, app.revanced.extension.shared.StringRef skippedToastMiddle, app.revanced.extension.shared.StringRef skippedToastEnd, app.revanced.extension.shared.settings.StringSetting behavior, app.revanced.extension.shared.settings.StringSetting color) {
        this.keyValue = java.util.Objects.requireNonNull(keyValue);
        this.title = java.util.Objects.requireNonNull(title);
        this.description = java.util.Objects.requireNonNull(description);
        this.skipButtonTextBeginning = java.util.Objects.requireNonNull(skipButtonTextBeginning);
        this.skipButtonTextMiddle = java.util.Objects.requireNonNull(skipButtonTextMiddle);
        this.skipButtonTextEnd = java.util.Objects.requireNonNull(skipButtonTextEnd);
        this.skippedToastBeginning = java.util.Objects.requireNonNull(skippedToastBeginning);
        this.skippedToastMiddle = java.util.Objects.requireNonNull(skippedToastMiddle);
        this.skippedToastEnd = java.util.Objects.requireNonNull(skippedToastEnd);
        this.behaviorSetting = java.util.Objects.requireNonNull(behavior);
        this.colorSetting = java.util.Objects.requireNonNull(color);
        this.paint = new android.graphics.Paint();
        loadFromSettings();
    }
    private void loadFromSettings() {
        java.lang.String behaviorString = behaviorSetting.get();
        app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour savedBehavior = app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.byReVancedKeyValue(behaviorString);
        if (((savedBehavior == null))) {
            app.revanced.extension.shared.Logger.printException(() -> ("Invalid behavior: " + behaviorString));
            behaviorSetting.resetToDefault();
            loadFromSettings();
            return;
        }
        this.behaviour = savedBehavior;
        java.lang.String colorString = colorSetting.get();
        try {
            setColorWithOpacity(colorString);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> ("Invalid color: " + colorString), ex);
            colorSetting.resetToDefault();
            loadFromSettings();
        }
    }
    public void setBehaviour(app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour) {
        this.behaviour = java.util.Objects.requireNonNull(behaviour);
        this.behaviorSetting.save(behaviour.reVancedKeyValue);
    }
    public void setColorWithOpacity(java.lang.String colorString) throws java.lang.IllegalArgumentException {
        int colorWithOpacity = android.graphics.Color.parseColor(colorString);
        colorSetting.save(java.lang.String.format(java.util.Locale.US, "#%08X", colorWithOpacity));
        color = colorWithOpacity;
        paint.setColor(color);
    }
    public void setOpacity(double opacity) {
        color = android.graphics.Color.argb(((int)((opacity * 255))), android.graphics.Color.red(color), android.graphics.Color.green(color), android.graphics.Color.blue(color));
        paint.setColor(color);
    }
    public int getColorWithOpacity() {
        return color;
    }
    public int getDefaultColorWithOpacity() {
        return android.graphics.Color.parseColor(colorSetting.defaultValue);
    }
    public java.lang.String getColorStringWithOpacity() {
        return java.lang.String.format(java.util.Locale.US, "#%08X", getColorWithOpacity());
    }
    public java.lang.String getColorStringWithoutOpacity() {
        final int colorNoOpacity = (getColorWithOpacity() & 16777215);
        return java.lang.String.format(java.util.Locale.US, "#%06X", colorNoOpacity);
    }
    public double getOpacity() {
        double opacity = (android.graphics.Color.alpha(color) / 255.0d);
        return (java.lang.Math.round((opacity * 100.0d)) / 100.0d);
    }
    public app.revanced.extension.shared.StringRef getTitle() {
        return title;
    }
    private static android.text.SpannableString getCategoryColorDotSpan(java.lang.String text, int color) {
        android.text.SpannableString dotSpan = new android.text.SpannableString((COLOR_DOT_STRING + text));
        dotSpan.setSpan(new android.text.style.ForegroundColorSpan(color), 0, 1, 33);
        dotSpan.setSpan(new android.text.style.RelativeSizeSpan(1.5f), 0, 1, 33);
        return dotSpan;
    }
    public android.text.SpannableString getTitleWithColorDot(int categoryColor) {
        return app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.getCategoryColorDotSpan((" " + title), categoryColor);
    }
    public android.text.SpannableString getTitleWithColorDot() {
        return getTitleWithColorDot(color);
    }
     app.revanced.extension.shared.StringRef getSkipButtonText(long segmentStartTime, long videoLength) {
        if ((app.revanced.extension.youtube.settings.Settings.SB_COMPACT_SKIP_BUTTON.get())) {
            return (((this == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT)) ? skipSponsorTextCompactHighlight : skipSponsorTextCompact);
        }
        if (((videoLength == 0))) {
            return skipButtonTextBeginning;
        }
        final float position = (segmentStartTime / ((float)videoLength));
        if (((position < 0.25f))) {
            return skipButtonTextBeginning;
        } else {
            if (((position < 0.75f))) {
                return skipButtonTextMiddle;
            }
        }
        return skipButtonTextEnd;
    }
     app.revanced.extension.shared.StringRef getSkippedToastText(long segmentStartTime, long videoLength) {
        if (((videoLength == 0))) {
            return skippedToastBeginning;
        }
        final float position = (segmentStartTime / ((float)videoLength));
        if (((position < 0.25f))) {
            return skippedToastBeginning;
        } else {
            if (((position < 0.75f))) {
                return skippedToastMiddle;
            }
        }
        return skippedToastEnd;
    }
}
