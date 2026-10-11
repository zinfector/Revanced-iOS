package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class DisableResumingStartupShortsPlayerPatch {
    public static boolean disableResumingStartupShortsPlayer() {
        return app.revanced.extension.youtube.settings.Settings.DISABLE_RESUMING_SHORTS_PLAYER.get();
    }
    public static boolean disableResumingStartupShortsPlayer(boolean original) {
        return (original && (!app.revanced.extension.youtube.settings.Settings.DISABLE_RESUMING_SHORTS_PLAYER.get()));
    }
}
