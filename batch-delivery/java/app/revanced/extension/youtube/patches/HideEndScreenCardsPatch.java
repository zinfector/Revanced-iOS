package app.revanced.extension.youtube.patches;
import android.view.View;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class HideEndScreenCardsPatch {
    public static void hideEndScreenCardView(android.view.View view) {
        app.revanced.extension.shared.Utils.hideViewUnderCondition(app.revanced.extension.youtube.settings.Settings.HIDE_END_SCREEN_CARDS, view);
    }
    public static boolean hideEndScreenCards() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_END_SCREEN_CARDS.get();
    }
}
