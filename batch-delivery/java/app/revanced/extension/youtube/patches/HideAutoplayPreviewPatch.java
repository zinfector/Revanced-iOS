package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class HideAutoplayPreviewPatch {
    public static boolean hideAutoplayPreview() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_AUTOPLAY_PREVIEW.get();
    }
}
