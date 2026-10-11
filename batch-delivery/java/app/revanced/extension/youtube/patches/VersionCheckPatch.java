package app.revanced.extension.youtube.patches;
import app.revanced.extension.shared.Utils;
// Generated from attributed editable regions, no source-body copying.
public class VersionCheckPatch {
    private static boolean isVersionOrGreater(java.lang.String version) {
        return (app.revanced.extension.shared.Utils.getAppVersionName().compareTo(version) >= 0);
    }
    final public static boolean IS_19_17_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.17.00");
    final public static boolean IS_19_20_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.20.00");
    final public static boolean IS_19_21_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.21.00");
    final public static boolean IS_19_26_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.26.00");
    final public static boolean IS_19_29_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.29.00");
    final public static boolean IS_19_34_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("19.34.00");
    final public static boolean IS_20_21_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("20.21.00");
    final public static boolean IS_20_22_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("20.22.00");
    final public static boolean IS_20_31_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("20.31.00");
    final public static boolean IS_20_37_OR_GREATER = app.revanced.extension.youtube.patches.VersionCheckPatch.isVersionOrGreater("20.37.00");
}
