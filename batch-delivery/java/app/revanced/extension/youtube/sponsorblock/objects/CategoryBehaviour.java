package app.revanced.extension.youtube.sponsorblock.objects;
import static app.revanced.extension.shared.StringRef.sf;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.Objects;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.StringRef;
// Generated from attributed editable regions, no source-body copying.
public enum CategoryBehaviour {
    SKIP_AUTOMATICALLY("skip", 2, true, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_automatically")),
SKIP_AUTOMATICALLY_ONCE("skip-once", 3, true, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_automatically_once")),
MANUAL_SKIP("manual-skip", 1, false, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_showbutton")),
SHOW_IN_SEEKBAR("seekbar-only", 0, false, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_seekbaronly")),
IGNORE("ignore", -1, false, app.revanced.extension.shared.StringRef.sf("revanced_sb_skip_ignore"));
    final public java.lang.String reVancedKeyValue;
    final public int desktopKeyValue;
    final public boolean skipAutomatically;
    final public app.revanced.extension.shared.StringRef description;
    private CategoryBehaviour(java.lang.String reVancedKeyValue, int desktopKeyValue, boolean skipAutomatically, app.revanced.extension.shared.StringRef description) {
        this.reVancedKeyValue = java.util.Objects.requireNonNull(reVancedKeyValue);
        this.desktopKeyValue = desktopKeyValue;
        this.skipAutomatically = skipAutomatically;
        this.description = java.util.Objects.requireNonNull(description);
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour byReVancedKeyValue(java.lang.String keyValue) {
        for (app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour : app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.values()) {
            if ((behaviour.reVancedKeyValue.equals(keyValue))) {
                return behaviour;
            }
        }
        return null;
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour byDesktopKeyValue(int desktopKeyValue) {
        for (app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour : app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.values()) {
            if (((behaviour.desktopKeyValue == desktopKeyValue))) {
                return behaviour;
            }
        }
        return null;
    }
    private static java.lang.String[] behaviorKeyValues;
    private static java.lang.String[] behaviorDescriptions;
    private static java.lang.String[] behaviorKeyValuesWithoutSkipOnce;
    private static java.lang.String[] behaviorDescriptionsWithoutSkipOnce;
    private static void createNameAndKeyArrays() {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour[] behaviours = app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.values();
        final int behaviorLength = behaviours.length;
        behaviorKeyValues = new java.lang.String[behaviorLength];
        behaviorDescriptions = new java.lang.String[behaviorLength];
        behaviorKeyValuesWithoutSkipOnce = new java.lang.String[(behaviorLength - 1)];
        behaviorDescriptionsWithoutSkipOnce = new java.lang.String[(behaviorLength - 1)];
        int behaviorIndex = 0;
        int behaviorHighlightIndex = 0;
        while (((behaviorIndex < behaviorLength))) {
            app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour = behaviours[behaviorIndex];
            java.lang.String value = behaviour.reVancedKeyValue;
            java.lang.String description = behaviour.description.toString();
            behaviorKeyValues[behaviorIndex] = value;
            behaviorDescriptions[behaviorIndex] = description;
            behaviorIndex++;
            if (((behaviour != SKIP_AUTOMATICALLY_ONCE))) {
                behaviorKeyValuesWithoutSkipOnce[behaviorHighlightIndex] = value;
                behaviorDescriptionsWithoutSkipOnce[behaviorHighlightIndex] = description;
                behaviorHighlightIndex++;
            }
        }
    }
    static java.lang.String[] getBehaviorKeyValues() {
        if (((behaviorKeyValues == null))) {
            app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.createNameAndKeyArrays();
        }
        return behaviorKeyValues;
    }
    static java.lang.String[] getBehaviorKeyValuesWithoutSkipOnce() {
        if (((behaviorKeyValuesWithoutSkipOnce == null))) {
            app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.createNameAndKeyArrays();
        }
        return behaviorKeyValuesWithoutSkipOnce;
    }
    static java.lang.String[] getBehaviorDescriptions() {
        if (((behaviorDescriptions == null))) {
            app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.createNameAndKeyArrays();
        }
        return behaviorDescriptions;
    }
    static java.lang.String[] getBehaviorDescriptionsWithoutSkipOnce() {
        if (((behaviorDescriptionsWithoutSkipOnce == null))) {
            app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.createNameAndKeyArrays();
        }
        return behaviorDescriptionsWithoutSkipOnce;
    }
}
