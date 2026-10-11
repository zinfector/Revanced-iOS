package app.revanced.extension.youtube.patches.playback.speed;
import static app.revanced.extension.shared.StringRef.str;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class RememberPlaybackSpeedPatch {
    final private static long TOAST_DELAY_MILLISECONDS = 750;
    private static volatile boolean newVideoStarted;
    private static long lastTimeSpeedChanged;
    public static void newVideoStarted(app.revanced.extension.youtube.patches.VideoInformation.PlaybackController ignoredPlayerController) {
        app.revanced.extension.shared.Logger.printDebug(() -> "newVideoStarted");
        newVideoStarted = true;
    }
    public static void userSelectedPlaybackSpeed(float playbackSpeed) {
        try {
            if ((app.revanced.extension.youtube.settings.Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.get())) {
                playbackSpeed = java.lang.Math.min(playbackSpeed, app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.PLAYBACK_SPEED_MAXIMUM);
                final long now = rv.batch.Platform.wallMillis();
                lastTimeSpeedChanged = now;
                final float finalPlaybackSpeed = playbackSpeed;
                app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    if (((lastTimeSpeedChanged != now))) {
        return;
    }
    if (((app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.get() == finalPlaybackSpeed))) {
        return;
    }
    app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.save(finalPlaybackSpeed);
    if ((app.revanced.extension.youtube.settings.Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST.get())) {
        app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_remember_playback_speed_toast", ((finalPlaybackSpeed + "x"))));
    }
}, TOAST_DELAY_MILLISECONDS);
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "userSelectedPlaybackSpeed failure", ex);
        }
    }
    public static float getPlaybackSpeedOverride() {
        if ((newVideoStarted)) {
            newVideoStarted = false;
            final float defaultSpeed = app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.get();
            if (((defaultSpeed > 0))) {
                return defaultSpeed;
            }
        }
        return (-2.0f);
    }
}
