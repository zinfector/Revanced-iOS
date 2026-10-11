package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class HideRelatedVideoOverlayPatch {
    public static boolean hideRelatedVideoOverlay() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_RELATED_VIDEOS_OVERLAY.get();
    }
}
