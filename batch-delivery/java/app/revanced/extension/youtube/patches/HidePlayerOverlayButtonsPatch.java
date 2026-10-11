package app.revanced.extension.youtube.patches;
import static app.revanced.extension.shared.Utils.getResourceIdentifierOrThrow;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.ResourceType;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class HidePlayerOverlayButtonsPatch {
    final private static boolean HIDE_AUTOPLAY_BUTTON_ENABLED = app.revanced.extension.youtube.settings.Settings.HIDE_AUTOPLAY_BUTTON.get();
    public static boolean hideAutoplayButton() {
        return HIDE_AUTOPLAY_BUTTON_ENABLED;
    }
    public static int getCastButtonOverrideV2(int original) {
        return (app.revanced.extension.youtube.settings.Settings.HIDE_CAST_BUTTON.get() ? 8 : original);
    }
    public static boolean getCastButtonOverrideV2(boolean original) {
        if ((app.revanced.extension.youtube.settings.Settings.HIDE_CAST_BUTTON.get())) {
            return false;
        }
        return original;
    }
    public static void hideCaptionsButton(android.widget.ImageView imageView) {
        imageView.setVisibility((app.revanced.extension.youtube.settings.Settings.HIDE_CAPTIONS_BUTTON.get() ? 8 : 0));
    }
    public static void hideCollapseButton(android.widget.ImageView imageView) {
        if (((!app.revanced.extension.youtube.settings.Settings.HIDE_COLLAPSE_BUTTON.get()))) {
            return;
        }
        imageView.setImageResource(17170445);
        imageView.setImageAlpha(0);
        imageView.setEnabled(false);
        android.view.ViewGroup.LayoutParams layoutParams = imageView.getLayoutParams();
        if (((layoutParams instanceof android.widget.RelativeLayout.LayoutParams))) {
            android.widget.RelativeLayout.LayoutParams lp = new android.widget.RelativeLayout.LayoutParams(0, 0);
            imageView.setLayoutParams(lp);
        } else {
            app.revanced.extension.shared.Logger.printDebug(() -> ("Unknown collapse button layout params: " + layoutParams));
        }
    }
    public static void setTitleAnchorStartMargin(android.view.View titleAnchorView) {
        if (((!app.revanced.extension.youtube.settings.Settings.HIDE_COLLAPSE_BUTTON.get()))) {
            return;
        }
        android.view.ViewGroup.LayoutParams layoutParams = titleAnchorView.getLayoutParams();
        if (((layoutParams instanceof android.widget.RelativeLayout.LayoutParams relativeParams))) {
            relativeParams.setMarginStart(0);
        } else {
            app.revanced.extension.shared.Logger.printDebug(() -> ("Unknown title anchor layout params: " + layoutParams));
        }
    }
    final private static boolean HIDE_PLAYER_PREVIOUS_NEXT_BUTTONS_ENABLED = app.revanced.extension.youtube.settings.Settings.HIDE_PLAYER_PREVIOUS_NEXT_BUTTONS.get();
    final private static int PLAYER_CONTROL_PREVIOUS_BUTTON_TOUCH_AREA_ID = app.revanced.extension.shared.Utils.getResourceIdentifierOrThrow(app.revanced.extension.shared.ResourceType.ID, "player_control_previous_button_touch_area");
    final private static int PLAYER_CONTROL_NEXT_BUTTON_TOUCH_AREA_ID = app.revanced.extension.shared.Utils.getResourceIdentifierOrThrow(app.revanced.extension.shared.ResourceType.ID, "player_control_next_button_touch_area");
    public static void hidePreviousNextButtons(android.view.View parentView) {
        if (((!HIDE_PLAYER_PREVIOUS_NEXT_BUTTONS_ENABLED))) {
            return;
        }
        app.revanced.extension.shared.Utils.runOnMainThread(() -> {
    app.revanced.extension.youtube.patches.HidePlayerOverlayButtonsPatch.hideView(parentView, PLAYER_CONTROL_PREVIOUS_BUTTON_TOUCH_AREA_ID);
    app.revanced.extension.youtube.patches.HidePlayerOverlayButtonsPatch.hideView(parentView, PLAYER_CONTROL_NEXT_BUTTON_TOUCH_AREA_ID);
});
    }
    public static android.view.View hideFullscreenButton(android.view.View view) {
        if (((!app.revanced.extension.youtube.settings.Settings.HIDE_FULLSCREEN_BUTTON.get()))) {
            return view;
        }
        if (((view != null))) {
            view.setVisibility(8);
        }
        return null;
    }
    public static void hidePlayerControlButtonsBackground(android.view.View rootView) {
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.HIDE_PLAYER_CONTROL_BUTTONS_BACKGROUND.get()))) {
                return;
            }
            app.revanced.extension.youtube.patches.HidePlayerOverlayButtonsPatch.removeImageViewsBackgroundRecursive(rootView);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "removePlayerControlButtonsBackground failure", ex);
        }
    }
    private static void hideView(android.view.View parentView, int resourceId) {
        android.view.View nextPreviousButton = parentView.findViewById(resourceId);
        if (((nextPreviousButton == null))) {
            app.revanced.extension.shared.Logger.printException(() -> "Could not find player previous/next button");
            return;
        }
        app.revanced.extension.shared.Logger.printDebug(() -> "Hiding previous/next button");
        app.revanced.extension.shared.Utils.hideViewByRemovingFromParentUnderCondition(true, nextPreviousButton);
    }
    private static void removeImageViewsBackgroundRecursive(android.view.View currentView) {
        if (((currentView instanceof android.widget.ImageView imageView))) {
            imageView.setBackground(null);
        }
        if (((currentView instanceof android.view.ViewGroup viewGroup))) {
            for (int i = 0; (i < viewGroup.getChildCount()); i++) {
                app.revanced.extension.youtube.patches.HidePlayerOverlayButtonsPatch.removeImageViewsBackgroundRecursive(viewGroup.getChildAt(i));
            }
        }
    }
}
