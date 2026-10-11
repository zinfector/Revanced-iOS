package app.revanced.extension.youtube.patches;
import android.view.View;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class HideInfoCardsPatch {
    public static void hideInfoCardsIncognito(android.view.View view) {
        if (((!app.revanced.extension.youtube.settings.Settings.HIDE_INFO_CARDS.get()))) {
            return;
        }
        view.setVisibility(8);
    }
    public static boolean hideInfoCardsMethodCall() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_INFO_CARDS.get();
    }
}
