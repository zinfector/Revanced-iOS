package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.PlayerType;
import app.revanced.extension.youtube.shared.ShortsPlayerState;
// Generated from attributed editable regions, no source-body copying.
public class BackgroundPlaybackPatch {
    public static boolean isBackgroundPlaybackAllowed(boolean original) {
        if ((original)) {
            return true;
        }
        if ((app.revanced.extension.youtube.shared.ShortsPlayerState.isOpen())) {
            return false;
        }
        app.revanced.extension.youtube.shared.PlayerType current = app.revanced.extension.youtube.shared.PlayerType.getCurrent();
        return ((!current.isNoneOrHidden()) && (current != app.revanced.extension.youtube.shared.PlayerType.INLINE_MINIMAL));
    }
    public static boolean isBackgroundShortsPlaybackAllowed(boolean original) {
        return (!app.revanced.extension.youtube.settings.Settings.DISABLE_SHORTS_BACKGROUND_PLAYBACK.get());
    }
}
