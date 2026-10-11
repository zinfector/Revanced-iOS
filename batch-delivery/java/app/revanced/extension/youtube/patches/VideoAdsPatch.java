package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class VideoAdsPatch {
    final private static boolean SHOW_VIDEO_ADS = (!app.revanced.extension.youtube.settings.Settings.HIDE_VIDEO_ADS.get());
    public static boolean shouldShowAds() {
        return SHOW_VIDEO_ADS;
    }
    public static java.lang.String hideShortsAds(java.lang.String osName) {
        return (SHOW_VIDEO_ADS ? osName : "Android Automotive");
    }
}
