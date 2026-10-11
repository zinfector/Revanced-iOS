package app.revanced.extension.youtube.patches.playback.speed;
import static app.revanced.extension.shared.StringRef.str;
import static app.revanced.extension.youtube.videoplayer.PlayerControlButton.fadeInDuration;
import static app.revanced.extension.youtube.videoplayer.PlayerControlButton.getDialogBackgroundColor;
import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import rv.batch.format.NumberFormat;
import android.support.v7.widget.RecyclerView;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.function.Function;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.ui.Dim;
import app.revanced.extension.shared.ui.SheetBottomDialog;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.PlayerType;
import kotlin.Unit;
import kotlin.jvm.functions.Function1;
// Generated from attributed editable regions, no source-body copying.
public class CustomPlaybackSpeedPatch {
    final public static float PLAYBACK_SPEED_MAXIMUM = 8;
    final private static double SPEED_ADJUSTMENT_CHANGE = 0.05d;
    final private static float PROGRESS_BAR_VALUE_SCALE = 100;
    final private static float TAP_AND_HOLD_SPEED;
    final public static float[] customPlaybackSpeeds;
    final private static float customPlaybackSpeedsMin;
    final private static float customPlaybackSpeedsMax;
    private static volatile long lastTimeOldPlaybackMenuInvoked;
    final private static rv.batch.format.NumberFormat speedFormatter = rv.batch.format.NumberFormat.getNumberInstance();
    private static java.lang.ref.WeakReference<app.revanced.extension.shared.ui.SheetBottomDialog.SlideDialog> currentDialog;
    static {
        speedFormatter.setMinimumFractionDigits(2);
        speedFormatter.setMaximumFractionDigits(2);
        final float holdSpeed = app.revanced.extension.youtube.settings.Settings.SPEED_TAP_AND_HOLD.get();
        if ((((holdSpeed > 0) && (holdSpeed <= PLAYBACK_SPEED_MAXIMUM)))) {
            TAP_AND_HOLD_SPEED = holdSpeed;
        } else {
            app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.showInvalidCustomSpeedToast();
            TAP_AND_HOLD_SPEED = app.revanced.extension.youtube.settings.Settings.SPEED_TAP_AND_HOLD.resetToDefault();
        }
        customPlaybackSpeeds = app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.loadCustomSpeeds();
        customPlaybackSpeedsMin = customPlaybackSpeeds[0];
        customPlaybackSpeedsMax = customPlaybackSpeeds[(customPlaybackSpeeds.length - 1)];
    }
    public static float getTapAndHoldSpeed() {
        return TAP_AND_HOLD_SPEED;
    }
    private static void showInvalidCustomSpeedToast() {
        app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_custom_playback_speeds_invalid", PLAYBACK_SPEED_MAXIMUM));
    }
    private static float[] loadCustomSpeeds() {
        try {
            java.lang.String[] speedStrings = app.revanced.extension.youtube.settings.Settings.CUSTOM_PLAYBACK_SPEEDS.get().replace(',', '.').split("\\s+");
            java.util.Arrays.sort(speedStrings);
            if (((speedStrings.length == 0))) {
                throw new java.lang.IllegalArgumentException();
            }
            float[] speeds = new float[speedStrings.length];
            int i = 0;
            for (java.lang.String speedString : speedStrings) {
                final float speedFloat = java.lang.Float.parseFloat(speedString);
                if ((((speedFloat <= 0) || app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.arrayContains(speeds, speedFloat)))) {
                    throw new java.lang.IllegalArgumentException();
                }
                if (((speedFloat > PLAYBACK_SPEED_MAXIMUM))) {
                    app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.showInvalidCustomSpeedToast();
                    app.revanced.extension.youtube.settings.Settings.CUSTOM_PLAYBACK_SPEEDS.resetToDefault();
                    return app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.loadCustomSpeeds();
                }
                speeds[i++] = speedFloat;
            }
            return speeds;
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "Parse error", ex);
            app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_custom_playback_speeds_parse_exception"));
            app.revanced.extension.youtube.settings.Settings.CUSTOM_PLAYBACK_SPEEDS.resetToDefault();
            return app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.loadCustomSpeeds();
        }
    }
    private static boolean arrayContains(float[] array, float value) {
        for (float arrayValue : array) {
            if (((arrayValue == value))) {
                return true;
            }
        }
        return false;
    }
    public static void onFlyoutMenuCreate(android.support.v7.widget.RecyclerView recyclerView) {
        recyclerView.getViewTreeObserver().addOnDrawListener(() -> {
    try {
        if ((app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter.isPlaybackRateSelectorMenuVisible)) {
            if ((app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.hideLithoMenuAndShowSpeedMenu(recyclerView, 5))) {
                app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter.isPlaybackRateSelectorMenuVisible = false;
            }
        }
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "isPlaybackRateSelectorMenuVisible failure", ex);
    }
    try {
        if ((app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter.isOldPlaybackSpeedMenuVisible)) {
            if ((app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.hideLithoMenuAndShowSpeedMenu(recyclerView, 8))) {
                app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter.isOldPlaybackSpeedMenuVisible = false;
            }
        }
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "isOldPlaybackSpeedMenuVisible failure", ex);
    }
});
    }
    private static boolean hideLithoMenuAndShowSpeedMenu(android.support.v7.widget.RecyclerView recyclerView, int expectedChildCount) {
        if (((recyclerView.getChildCount() == 0))) {
            return false;
        }
        if (((!((recyclerView.getChildAt(0) instanceof android.view.ViewGroup playbackSpeedParentView))))) {
            return false;
        }
        if (((playbackSpeedParentView.getChildCount() != expectedChildCount))) {
            return false;
        }
        if (((!((app.revanced.extension.shared.Utils.getParentView(recyclerView, 3) instanceof android.view.ViewGroup parentView3rd))))) {
            return false;
        }
        if (((!((parentView3rd.getParent() instanceof android.view.ViewGroup parentView4th))))) {
            return false;
        }
        android.view.View touchInsidedView = parentView4th.getChildAt(0);
        touchInsidedView.setSoundEffectsEnabled(false);
        touchInsidedView.performClick();
        parentView3rd.setVisibility(8);
        parentView4th.setVisibility(8);
        if ((app.revanced.extension.youtube.settings.Settings.RESTORE_OLD_SPEED_MENU.get())) {
            app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.showOldPlaybackSpeedMenu();
            app.revanced.extension.shared.Logger.printDebug(() -> "Old playback speed dialog shown");
        } else {
            app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.showModernCustomPlaybackSpeedDialog(recyclerView.getContext());
            app.revanced.extension.shared.Logger.printDebug(() -> "Modern playback speed dialog shown");
        }
        return true;
    }
    public static void showOldPlaybackSpeedMenu() {
        final long now = rv.batch.Platform.wallMillis();
        if ((((now - lastTimeOldPlaybackMenuInvoked) < 1000))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Ignoring call to showOldPlaybackSpeedMenu");
            return;
        }
        lastTimeOldPlaybackMenuInvoked = now;
    }
    public static void showModernCustomPlaybackSpeedDialog(android.content.Context context) {
        try {
            app.revanced.extension.shared.ui.SheetBottomDialog.DraggableLinearLayout mainLayout = app.revanced.extension.shared.ui.SheetBottomDialog.createMainLayout(context, app.revanced.extension.youtube.videoplayer.PlayerControlButton.getDialogBackgroundColor());
            android.widget.TextView currentSpeedText = new android.widget.TextView(context);
            float currentSpeed = app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed();
            currentSpeedText.setText(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.formatSpeedStringX(currentSpeed));
            currentSpeedText.setTextColor(app.revanced.extension.shared.Utils.getAppForegroundColor());
            currentSpeedText.setTextSize(16);
            currentSpeedText.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
            currentSpeedText.setGravity(17);
            android.widget.LinearLayout.LayoutParams textParams = new android.widget.LinearLayout.LayoutParams(-1, -2);
            textParams.setMargins(0, 20, 0, 0);
            currentSpeedText.setLayoutParams(textParams);
            mainLayout.addView(currentSpeedText);
            android.widget.LinearLayout sliderLayout = new android.widget.LinearLayout(context);
            sliderLayout.setOrientation(0);
            sliderLayout.setGravity(16);
            android.widget.Button minusButton = app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.createStyledButton(context, false);
            android.widget.Button plusButton = app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.createStyledButton(context, true);
            android.widget.SeekBar speedSlider = new android.widget.SeekBar(context);
            speedSlider.setFocusable(true);
            speedSlider.setFocusableInTouchMode(true);
            speedSlider.setMax(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.speedToProgressValue(customPlaybackSpeedsMax));
            speedSlider.setProgress(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.speedToProgressValue(currentSpeed));
            speedSlider.getProgressDrawable().setColorFilter(app.revanced.extension.shared.Utils.getAppForegroundColor(), android.graphics.PorterDuff.Mode.SRC_IN);
            speedSlider.getThumb().setColorFilter(app.revanced.extension.shared.Utils.getAppForegroundColor(), android.graphics.PorterDuff.Mode.SRC_IN);
            android.widget.LinearLayout.LayoutParams sliderParams = new android.widget.LinearLayout.LayoutParams(0, -2, 1.0f);
            speedSlider.setLayoutParams(sliderParams);
            sliderLayout.addView(minusButton);
            sliderLayout.addView(speedSlider);
            sliderLayout.addView(plusButton);
            mainLayout.addView(sliderLayout);
            java.util.function.Function<java.lang.Float,java.lang.Void> userSelectedSpeed = (java.lang.Float newSpeed) -> {
    final float roundedSpeed = app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.roundSpeedToNearestIncrement(newSpeed);
    if (((app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed() == roundedSpeed))) {
        return null;
    }
    currentSpeedText.setText(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.formatSpeedStringX(roundedSpeed));
    speedSlider.setProgress(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.speedToProgressValue(roundedSpeed));
    app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch.userSelectedPlaybackSpeed(roundedSpeed);
    app.revanced.extension.youtube.patches.VideoInformation.overridePlaybackSpeed(roundedSpeed);
    return null;
};
            speedSlider.setOnSeekBarChangeListener(new android.widget.SeekBar.OnSeekBarChangeListener(){
    public void onProgressChanged(android.widget.SeekBar seekBar, int progress, boolean fromUser) {
        if ((fromUser)) {
            userSelectedSpeed.apply((customPlaybackSpeedsMin + ((progress / PROGRESS_BAR_VALUE_SCALE))));
        }
    }
    public void onStartTrackingTouch(android.widget.SeekBar seekBar) {
    }
    public void onStopTrackingTouch(android.widget.SeekBar seekBar) {
    }
});
            minusButton.setOnClickListener((android.view.View v) -> userSelectedSpeed.apply(((float)((app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed() - SPEED_ADJUSTMENT_CHANGE)))));
            plusButton.setOnClickListener((android.view.View v) -> userSelectedSpeed.apply(((float)((app.revanced.extension.youtube.patches.VideoInformation.getPlaybackSpeed() + SPEED_ADJUSTMENT_CHANGE)))));
            android.widget.GridLayout gridLayout = new android.widget.GridLayout(context);
            gridLayout.setColumnCount(5);
            gridLayout.setAlignmentMode(0);
            gridLayout.setRowCount(((int)java.lang.Math.ceil((customPlaybackSpeeds.length / 5.0d))));
            android.widget.LinearLayout.LayoutParams gridParams = new android.widget.LinearLayout.LayoutParams(-1, -2);
            gridParams.setMargins(4, 12, 4, 12);
            gridLayout.setLayoutParams(gridParams);
            speedFormatter.setMinimumFractionDigits(1);
            for (float speed : customPlaybackSpeeds) {
                android.widget.FrameLayout buttonContainer = new android.widget.FrameLayout(context);
                android.widget.GridLayout.LayoutParams containerParams = new android.widget.GridLayout.LayoutParams();
                containerParams.width = 0;
                containerParams.columnSpec = android.widget.GridLayout.spec(-2147483648, 1, 1.0f);
                containerParams.setMargins(4, 0, 4, 0);
                containerParams.height = app.revanced.extension.shared.ui.Dim.dp(60);
                buttonContainer.setLayoutParams(containerParams);
                android.widget.Button speedButton = new android.widget.Button(context, null, 0);
                speedButton.setText(speedFormatter.format(speed));
                speedButton.setTextColor(app.revanced.extension.shared.Utils.getAppForegroundColor());
                speedButton.setTextSize(12);
                speedButton.setAllCaps(false);
                speedButton.setGravity(17);
                android.graphics.drawable.ShapeDrawable buttonBackground = new android.graphics.drawable.ShapeDrawable(new android.graphics.drawable.shapes.RoundRectShape(app.revanced.extension.shared.ui.Dim.roundedCorners(20), null, null));
                buttonBackground.getPaint().setColor(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.getAdjustedBackgroundColor(false));
                speedButton.setBackground(buttonBackground);
                speedButton.setPadding(4, 4, 4, 4);
                android.widget.FrameLayout.LayoutParams buttonParams = new android.widget.FrameLayout.LayoutParams(-1, 32, 17);
                speedButton.setLayoutParams(buttonParams);
                buttonContainer.addView(speedButton);
                if (((speed == 1.0f))) {
                    android.widget.TextView normalLabel = new android.widget.TextView(context);
                    normalLabel.setText(app.revanced.extension.shared.StringRef.str("normal_playback_rate_label"));
                    normalLabel.setTextColor(app.revanced.extension.shared.Utils.getAppForegroundColor());
                    normalLabel.setTextSize(10);
                    normalLabel.setGravity(17);
                    android.widget.FrameLayout.LayoutParams labelParams = new android.widget.FrameLayout.LayoutParams(-2, -2, (80 | 1));
                    labelParams.bottomMargin = 0;
                    normalLabel.setLayoutParams(labelParams);
                    buttonContainer.addView(normalLabel);
                }
                speedButton.setOnClickListener((android.view.View v) -> userSelectedSpeed.apply(speed));
                gridLayout.addView(buttonContainer);
            }
            speedFormatter.setMinimumFractionDigits(2);
            mainLayout.addView(gridLayout);
            app.revanced.extension.shared.ui.SheetBottomDialog.SlideDialog dialog = app.revanced.extension.shared.ui.SheetBottomDialog.createSlideDialog(context, mainLayout, 200);
            currentDialog = new java.lang.ref.WeakReference<>(dialog);
            kotlin.jvm.functions.Function1<app.revanced.extension.youtube.shared.PlayerType,kotlin.Unit> playerTypeObserver = new kotlin.jvm.functions.Function1<>(){
    public kotlin.Unit invoke(app.revanced.extension.youtube.shared.PlayerType type) {
        app.revanced.extension.shared.ui.SheetBottomDialog.SlideDialog current = currentDialog.get();
        if ((((current == null) || (!current.isShowing())))) {
            app.revanced.extension.youtube.shared.PlayerType.getOnChange().removeObserver(this);
            app.revanced.extension.shared.Logger.printException(() -> "Removing player type listener as dialog is null or closed");
        } else {
            if (((type == app.revanced.extension.youtube.shared.PlayerType.WATCH_WHILE_PICTURE_IN_PICTURE))) {
                current.dismiss();
                app.revanced.extension.shared.Logger.printDebug(() -> "Playback speed dialog dismissed due to PiP mode");
            }
        }
        return kotlin.Unit.INSTANCE;
    }
};
            app.revanced.extension.youtube.shared.PlayerType.getOnChange().addObserver(playerTypeObserver);
            dialog.setOnDismissListener((android.content.DialogInterface d) -> {
    app.revanced.extension.youtube.shared.PlayerType.getOnChange().removeObserver(playerTypeObserver);
    app.revanced.extension.shared.Logger.printDebug(() -> "PlayerType observer removed on dialog dismiss");
});
            dialog.show();
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "showModernCustomPlaybackSpeedDialog failure", ex);
        }
    }
    private static android.widget.Button createStyledButton(android.content.Context context, boolean isPlus) {
        android.widget.Button button = new android.widget.Button(context, null, 0);
        button.setText("");
        android.graphics.drawable.ShapeDrawable background = new android.graphics.drawable.ShapeDrawable(new android.graphics.drawable.shapes.RoundRectShape(app.revanced.extension.shared.ui.Dim.roundedCorners(20), null, null));
        background.getPaint().setColor(app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.getAdjustedBackgroundColor(false));
        button.setBackground(background);
        button.setForeground(new app.revanced.extension.youtube.patches.playback.speed.OutlineSymbolDrawable(isPlus));
        android.widget.LinearLayout.LayoutParams params = new android.widget.LinearLayout.LayoutParams(36, 36);
        params.setMargins(8, 0, 8, 0);
        button.setLayoutParams(params);
        return button;
    }
    private static java.lang.String formatSpeedStringX(float speed) {
        return (speedFormatter.format(speed) + 'x');
    }
    private static int speedToProgressValue(float speed) {
        return ((int)((((speed - customPlaybackSpeedsMin)) * PROGRESS_BAR_VALUE_SCALE)));
    }
    private static float roundSpeedToNearestIncrement(float speed) {
        if ((app.revanced.extension.youtube.patches.playback.speed.CustomPlaybackSpeedPatch.arrayContains(customPlaybackSpeeds, speed))) {
            return speed;
        }
        final double roundedSpeed = (java.lang.Math.round((speed / SPEED_ADJUSTMENT_CHANGE)) * SPEED_ADJUSTMENT_CHANGE);
        return app.revanced.extension.shared.Utils.clamp(((float)roundedSpeed), ((float)SPEED_ADJUSTMENT_CHANGE), PLAYBACK_SPEED_MAXIMUM);
    }
    public static int getAdjustedBackgroundColor(boolean isHandleBar) {
        final float darkThemeFactor = (isHandleBar ? 1.25f : 1.115f);
        final float lightThemeFactor = (isHandleBar ? 0.9f : 0.95f);
        return app.revanced.extension.shared.Utils.adjustColorBrightness(app.revanced.extension.youtube.videoplayer.PlayerControlButton.getDialogBackgroundColor(), lightThemeFactor, darkThemeFactor);
    }
}
 class OutlineSymbolDrawable extends android.graphics.drawable.Drawable {
    final private boolean isPlus;
    final private android.graphics.Paint paint;
     OutlineSymbolDrawable(boolean isPlus) {
        this.isPlus = isPlus;
        paint = new android.graphics.Paint(1);
        paint.setColor(app.revanced.extension.shared.Utils.getAppForegroundColor());
        paint.setStyle(android.graphics.Paint.Style.STROKE);
        paint.setStrokeWidth(1);
    }
    public void draw(android.graphics.Canvas canvas) {
        android.graphics.Rect bounds = getBounds();
        final int width = bounds.width();
        final int height = bounds.height();
        final float centerX = (width / 2.0f);
        final float centerY = (height / 2.0f);
        final float size = (java.lang.Math.min(width, height) * 0.25f);
        canvas.drawLine((centerX - size), centerY, (centerX + size), centerY, paint);
        if ((isPlus)) {
            canvas.drawLine(centerX, (centerY - size), centerX, (centerY + size), paint);
        }
    }
    public void setAlpha(int alpha) {
        paint.setAlpha(alpha);
    }
    public void setColorFilter(android.graphics.ColorFilter colorFilter) {
        paint.setColorFilter(colorFilter);
    }
    public int getOpacity() {
        return -3;
    }
}
