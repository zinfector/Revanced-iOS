package app.revanced.extension.youtube.sponsorblock;
import static app.revanced.extension.shared.StringRef.str;
import android.app.Dialog;
import android.content.Context;
import android.util.Pair;
import android.util.Patterns;
import android.widget.LinearLayout;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.json.JSONArray;
import org.json.JSONObject;
import java.util.Locale;
import java.util.UUID;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.settings.Setting;
import app.revanced.extension.shared.ui.CustomDialog;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour;
import app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory;
import app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockPreferenceGroup;
// Generated from attributed editable regions, no source-body copying.
public class SponsorBlockSettings {
    final private static int SB_PRIVATE_USER_ID_MINIMUM_LENGTH = 30;
    final public static app.revanced.extension.shared.settings.Setting.ImportExportCallback SB_IMPORT_EXPORT_CALLBACK = new app.revanced.extension.shared.settings.Setting.ImportExportCallback(){
    public void settingsImported(android.content.Context context) {
        app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.loadAllCategoriesFromSettings();
        app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockPreferenceGroup.settingsImported = true;
    }
    public void settingsExported(android.content.Context context) {
        app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.showExportWarningIfNeeded(context);
    }
};
    public static void importDesktopSettings(java.lang.String json) {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        try {
            org.json.JSONObject settingsJson = new org.json.JSONObject(json);
            org.json.JSONObject barTypesObject = settingsJson.getJSONObject("barTypes");
            org.json.JSONArray categorySelectionsArray = settingsJson.getJSONArray("categorySelections");
            for (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category : app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutUnsubmitted()) {
                category.setBehaviour(app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE);
                if ((barTypesObject.has(category.keyValue))) {
                    org.json.JSONObject categoryObject = barTypesObject.getJSONObject(category.keyValue);
                    if (((categoryObject.has("color") && categoryObject.has("opacity")))) {
                        category.setColorWithOpacity(categoryObject.getString("color"));
                        category.setOpacity(((float)categoryObject.getDouble("opacity")));
                    }
                }
            }
            for (int i = 0, length = categorySelectionsArray.length(); (i < length); i++) {
                org.json.JSONObject categorySelectionObject = categorySelectionsArray.getJSONObject(i);
                java.lang.String categoryKey = categorySelectionObject.getString("name");
                app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.byCategoryKey(categoryKey);
                if (((category == null))) {
                    continue;
                }
                final int desktopValue = categorySelectionObject.getInt("option");
                app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour behaviour = app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.byDesktopKeyValue(desktopValue);
                if (((behaviour == null))) {
                    app.revanced.extension.shared.Utils.showToastLong(((categoryKey + " unknown behavior key: ") + categoryKey));
                } else {
                    if ((((category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT) && (behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.SKIP_AUTOMATICALLY_ONCE)))) {
                        app.revanced.extension.shared.Utils.showToastLong(("Skip-once behavior not allowed for " + category.keyValue));
                        category.setBehaviour(app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.SKIP_AUTOMATICALLY);
                    } else {
                        category.setBehaviour(behaviour);
                    }
                }
            }
            app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.updateEnabledCategories();
            if ((settingsJson.has("userID"))) {
                java.lang.String userID = settingsJson.getString("userID");
                if ((app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.isValidSBUserID(userID))) {
                    app.revanced.extension.youtube.settings.Settings.SB_PRIVATE_USER_ID.save(userID);
                }
            }
            app.revanced.extension.youtube.settings.Settings.SB_USER_IS_VIP.save(settingsJson.getBoolean("isVip"));
            app.revanced.extension.youtube.settings.Settings.SB_TOAST_ON_SKIP.save((!settingsJson.getBoolean("dontShowNotice")));
            app.revanced.extension.youtube.settings.Settings.SB_TRACK_SKIP_COUNT.save(settingsJson.getBoolean("trackViewCount"));
            app.revanced.extension.youtube.settings.Settings.SB_VIDEO_LENGTH_WITHOUT_SEGMENTS.save(settingsJson.getBoolean("showTimeWithSkips"));
            java.lang.String serverAddress = settingsJson.getString("serverAddress");
            if ((app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.isValidSBServerAddress(serverAddress))) {
                app.revanced.extension.youtube.settings.Settings.SB_API_URL.save(serverAddress);
            }
            final float minDuration = ((float)settingsJson.getDouble("minDuration"));
            if (((minDuration < 0))) {
                throw new java.lang.IllegalArgumentException(("invalid minDuration: " + minDuration));
            }
            app.revanced.extension.youtube.settings.Settings.SB_SEGMENT_MIN_DURATION.save(minDuration);
            if ((settingsJson.has("skipCount"))) {
                int skipCount = settingsJson.getInt("skipCount");
                if (((skipCount < 0))) {
                    throw new java.lang.IllegalArgumentException(("invalid skipCount: " + skipCount));
                }
                app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_NUMBER_SEGMENTS.save(skipCount);
            }
            if ((settingsJson.has("minutesSaved"))) {
                final double minutesSaved = settingsJson.getDouble("minutesSaved");
                if (((minutesSaved < 0))) {
                    throw new java.lang.IllegalArgumentException(("invalid minutesSaved: " + minutesSaved));
                }
                app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_MILLISECONDS.save(((long)(((minutesSaved * 60) * 1000))));
            }
            app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_settings_import_successful"));
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "failed to import settings", ex);
            app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_settings_import_failed", ex.getMessage()));
        }
    }
    public static java.lang.String exportDesktopSettings() {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        try {
            app.revanced.extension.shared.Logger.printDebug(() -> "Creating SponsorBlock export settings string");
            org.json.JSONObject json = new org.json.JSONObject();
            org.json.JSONObject barTypesObject = new org.json.JSONObject();
            org.json.JSONArray categorySelectionsArray = new org.json.JSONArray();
            app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory[] categories = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.categoriesWithoutUnsubmitted();
            for (app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category : categories) {
                org.json.JSONObject categoryObject = new org.json.JSONObject();
                java.lang.String categoryKey = category.keyValue;
                categoryObject.put("color", category.getColorStringWithoutOpacity());
                categoryObject.put("opacity", category.getOpacity());
                barTypesObject.put(categoryKey, categoryObject);
                if (((category.behaviour != app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.IGNORE))) {
                    org.json.JSONObject behaviorObject = new org.json.JSONObject();
                    behaviorObject.put("name", categoryKey);
                    behaviorObject.put("option", category.behaviour.desktopKeyValue);
                    categorySelectionsArray.put(behaviorObject);
                }
            }
            if ((app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.userHasSBPrivateID())) {
                json.put("userID", app.revanced.extension.youtube.settings.Settings.SB_PRIVATE_USER_ID.get());
            }
            json.put("isVip", app.revanced.extension.youtube.settings.Settings.SB_USER_IS_VIP.get());
            json.put("serverAddress", app.revanced.extension.youtube.settings.Settings.SB_API_URL.get());
            json.put("dontShowNotice", (!app.revanced.extension.youtube.settings.Settings.SB_TOAST_ON_SKIP.get()));
            json.put("showTimeWithSkips", app.revanced.extension.youtube.settings.Settings.SB_VIDEO_LENGTH_WITHOUT_SEGMENTS.get());
            json.put("minDuration", app.revanced.extension.youtube.settings.Settings.SB_SEGMENT_MIN_DURATION.get());
            json.put("trackViewCount", app.revanced.extension.youtube.settings.Settings.SB_TRACK_SKIP_COUNT.get());
            json.put("skipCount", app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_NUMBER_SEGMENTS.get());
            json.put("minutesSaved", (app.revanced.extension.youtube.settings.Settings.SB_LOCAL_TIME_SAVED_MILLISECONDS.get() / ((60.0f * 1000))));
            json.put("categorySelections", categorySelectionsArray);
            json.put("barTypes", barTypesObject);
            return json.toString(2);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "failed to export settings", ex);
            app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_settings_export_failed", ex));
            return "";
        }
    }
    private static void showExportWarningIfNeeded(android.content.Context dialogContext) {
        app.revanced.extension.shared.Utils.verifyOnMainThread();
        app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.initialize();
        if (((((dialogContext != null) && app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.userHasSBPrivateID()) && (!app.revanced.extension.youtube.settings.Settings.SB_HIDE_EXPORT_WARNING.get())))) {
            android.util.Pair<android.app.Dialog,android.widget.LinearLayout> dialogPair = app.revanced.extension.shared.ui.CustomDialog.create(dialogContext, null, app.revanced.extension.shared.StringRef.str("revanced_sb_settings_revanced_export_user_id_warning"), null, null, () -> {
}, null, app.revanced.extension.shared.StringRef.str("revanced_sb_settings_revanced_export_user_id_warning_dismiss"), () -> app.revanced.extension.youtube.settings.Settings.SB_HIDE_EXPORT_WARNING.save(true), true);
            dialogPair.first.setCancelable(false);
            dialogPair.first.show();
        }
    }
    public static boolean isValidSBUserID(java.lang.String userID) {
        return ((!userID.isEmpty()) && (userID.length() >= SB_PRIVATE_USER_ID_MINIMUM_LENGTH));
    }
    public static boolean isValidSBServerAddress(java.lang.String serverAddress) {
        if (((!android.util.Patterns.WEB_URL.matcher(serverAddress).matches()))) {
            return false;
        }
        final int lastDotIndex = serverAddress.lastIndexOf('.');
        return ((lastDotIndex > 0) && (!serverAddress.substring(lastDotIndex).contains("/")));
    }
    public static boolean userHasSBPrivateID() {
        return (!app.revanced.extension.youtube.settings.Settings.SB_PRIVATE_USER_ID.get().isEmpty());
    }
    public static java.lang.String getSBPrivateUserID() {
        java.lang.String uuid = app.revanced.extension.youtube.settings.Settings.SB_PRIVATE_USER_ID.get();
        if ((uuid.isEmpty())) {
            uuid = (((java.util.UUID.randomUUID().toString() + java.util.UUID.randomUUID().toString()) + java.util.UUID.randomUUID().toString())).replace("-", "");
            app.revanced.extension.youtube.settings.Settings.SB_PRIVATE_USER_ID.save(uuid);
        }
        return uuid;
    }
    public static java.lang.String migrateOldColorString(java.lang.String colorString, float opacity) {
        if (((colorString.length() >= 8))) {
            return colorString;
        }
        if ((colorString.startsWith("#"))) {
            colorString = colorString.substring(1);
        }
        java.lang.String alphaHex = java.lang.String.format(java.util.Locale.US, "%02X", ((int)((opacity * 255))));
        java.lang.String argbColorString = (('#' + alphaHex) + colorString.substring(0, 6));
        app.revanced.extension.shared.Logger.printDebug(() -> ("Migrating old color string with default opacity: " + argbColorString));
        return argbColorString;
    }
    private static boolean initialized;
    public static void initialize() {
        if ((initialized)) {
            return;
        }
        initialized = true;
        app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.updateEnabledCategories();
    }
}
