package app.revanced.extension.youtube.patches;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class DisablePlayerPopupPanelsPatch {
    public static boolean disablePlayerPopupPanels() {
        return app.revanced.extension.youtube.settings.Settings.DISABLE_PLAYER_POPUP_PANELS.get();
    }
}
