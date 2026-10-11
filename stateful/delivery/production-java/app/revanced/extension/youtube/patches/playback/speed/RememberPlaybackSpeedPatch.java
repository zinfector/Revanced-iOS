package app.revanced.extension.youtube.patches.playback.speed;

// Generated from checked editable regions.
final public class RememberPlaybackSpeedPatch {
    final private static long TOAST_DELAY_MILLISECONDS = 750;
    private static volatile boolean newVideoStarted;
    private static long lastTimeSpeedChanged;
    public static void newVideoStarted(app.revanced.extension.youtube.patches.VideoInformation.PlaybackController ignoredPlayerController) {
        app.revanced.extension.shared.Logger.printDebug(() -> "newVideoStarted");
        app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.newVideoStarted = true;
    }
    public static void userSelectedPlaybackSpeed(float playbackSpeed) {
        try {
            if ((app.revanced.extension.youtube.settings.Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.get())) {
                playbackSpeed = java.lang.Math.min(playbackSpeed, 8.0f);
                final long now = rv.translated.PlatformClock.wallMillis();
                app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.lastTimeSpeedChanged = now;
                final float finalPlaybackSpeed = playbackSpeed;
                app.revanced.extension.shared.Utils.runOnMainThreadDelayed(() -> {
    if (((app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.lastTimeSpeedChanged != now))) {
        return;
    }
    if (((app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.get() == finalPlaybackSpeed))) {
        return;
    }
    app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.save(finalPlaybackSpeed);
    if ((app.revanced.extension.youtube.settings.Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED_TOAST.get())) {
        app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_remember_playback_speed_toast", ((finalPlaybackSpeed + "x"))));
    }
}, app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.TOAST_DELAY_MILLISECONDS);
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "userSelectedPlaybackSpeed failure", ex);
        }
    }
    public static float getPlaybackSpeedOverride() {
        if ((app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.newVideoStarted)) {
            app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.newVideoStarted = false;
            final float defaultSpeed = app.revanced.extension.youtube.settings.Settings.PLAYBACK_SPEED_DEFAULT.get();
            if (((defaultSpeed > 0))) {
                return defaultSpeed;
            }
        }
        return (-2.0f);
    }
}
