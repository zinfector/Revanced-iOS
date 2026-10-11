package app.revanced.extension.youtube.sponsorblock.requests;
import static app.revanced.extension.shared.StringRef.str;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings;
import app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils;
import app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory;
import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment;
import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote;
import app.revanced.extension.youtube.sponsorblock.objects.UserStats;
import app.revanced.extension.shared.requests.Route;
import app.revanced.extension.shared.requests.Requester;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
// Generated from attributed editable regions, no source-body copying.
public class SBRequester {
    final private static java.lang.String TIME_TEMPLATE = "%.3f";
    final private static int TIMEOUT_TCP_DEFAULT_MILLISECONDS = 7000;
    final private static int TIMEOUT_HTTP_DEFAULT_MILLISECONDS = 10000;
    final private static int HTTP_STATUS_CODE_SUCCESS = 200;
    private static volatile app.revanced.extension.youtube.sponsorblock.objects.UserStats lastFetchedStats;
    private SBRequester() {
    }
    private static void handleConnectionError(java.lang.String toastMessage, java.lang.Exception ex) {
        if ((app.revanced.extension.youtube.settings.Settings.SB_TOAST_ON_CONNECTION_ERROR.get())) {
            app.revanced.extension.shared.Utils.showToastShort(toastMessage);
        }
        if (((ex != null))) {
            app.revanced.extension.shared.Logger.printInfo(() -> toastMessage, ex);
        }
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[] getSegments(java.lang.String videoId) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.util.List<app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment> segments = new java.util.ArrayList<>();
        try {
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.GET_SEGMENTS, videoId, app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.sponsorBlockAPIFetchCategories);
            final int responseCode = connection.getResponseCode();
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                org.json.JSONArray responseArray = app.revanced.extension.shared.requests.Requester.parseJSONArray(connection);
                final long minSegmentDuration = ((long)((app.revanced.extension.youtube.settings.Settings.SB_SEGMENT_MIN_DURATION.get() * 1000)));
                for (int i = 0, length = responseArray.length(); (i < length); i++) {
                    org.json.JSONObject obj = ((org.json.JSONObject)responseArray.get(i));
                    org.json.JSONArray segment = obj.getJSONArray("segment");
                    final long start = ((long)((segment.getDouble(0) * 1000)));
                    final long end = ((long)((segment.getDouble(1) * 1000)));
                    java.lang.String uuid = obj.getString("UUID");
                    final boolean locked = (obj.getInt("locked") == 1);
                    java.lang.String categoryKey = obj.getString("category");
                    app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category = app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.byCategoryKey(categoryKey);
                    if (((category == null))) {
                        app.revanced.extension.shared.Logger.printException(() -> ("Received unknown category: " + categoryKey));
                    } else {
                        if ((((((end - start)) >= minSegmentDuration) || (category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT)))) {
                            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(category, uuid, start, end, locked));
                        }
                    }
                }
                app.revanced.extension.shared.Logger.printDebug(() -> {
    java.lang.StringBuilder builder = new java.lang.StringBuilder("Downloaded segments:");
    for (app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment : segments) {
        builder.append('\n').append(segment);
    }
    return builder.toString();
});
                app.revanced.extension.youtube.sponsorblock.requests.SBRequester.runVipCheckInBackgroundIfNeeded();
            } else {
                if (((responseCode == 404))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("No segments found for video: " + videoId));
                } else {
                    app.revanced.extension.youtube.sponsorblock.requests.SBRequester.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_sb_sponsorblock_connection_failure_status", responseCode), null);
                    connection.disconnect();
                }
            }
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.sponsorblock.requests.SBRequester.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_sb_sponsorblock_connection_failure_timeout"), ex);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.sponsorblock.requests.SBRequester.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_sb_sponsorblock_connection_failure_generic"), ex);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "getSegments failure", ex);
        }
        if ((false)) {
            segments.clear();
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 5000, 120000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.SELF_PROMO, "debug", 10000, 60000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTERACTION, "debug", 15000, 20000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.SPONSOR, "debug", 21000, 30000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.OUTRO, "debug", 24000, 27000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.MUSIC_OFFTOPIC, "debug", 200000, 300000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.SPONSOR, "debug", 200000, 250000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.SELF_PROMO, "debug", 200000, 330000, false));
        }
        if ((false)) {
            segments.clear();
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 2000, 12000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.SELF_PROMO, "debug", 8000, 15000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 16000, 17000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 18000, 19000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 20000, 21000, false));
            segments.add(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.INTRO, "debug", 22000, 23000, false));
        }
        return segments.toArray(new app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment[0]);
    }
    public static void submitSegments(java.lang.String videoId, java.lang.String category, long startTime, long endTime, long videoLength) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        try {
            java.lang.String privateUserID = app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID();
            java.lang.String start = java.lang.String.format(java.util.Locale.US, TIME_TEMPLATE, (startTime / 1000.0f));
            java.lang.String end = java.lang.String.format(java.util.Locale.US, TIME_TEMPLATE, (endTime / 1000.0f));
            java.lang.String duration = java.lang.String.format(java.util.Locale.US, TIME_TEMPLATE, (videoLength / 1000.0f));
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.SUBMIT_SEGMENTS, privateUserID, videoId, category, start, end, duration);
            final int responseCode = connection.getResponseCode();
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_submit_succeeded"));
                return;
            }
            java.lang.String userErrorMessage = switch ((responseCode)) {    case 409 -> app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_duplicate");
    case 403 -> app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_forbidden", app.revanced.extension.shared.requests.Requester.parseErrorStringAndDisconnect(connection));
    case 429 -> app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_rate_limit");
    case 400 -> app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_invalid", app.revanced.extension.shared.requests.Requester.parseErrorStringAndDisconnect(connection));
    default -> app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_unknown_error", responseCode, connection.getResponseMessage());};
            app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.showErrorDialog(userErrorMessage);
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Timeout", ex);
            app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_timeout"));
        } catch (java.io.IOException ex) {
            app.revanced.extension.shared.Logger.printDebug(() -> "IOException", ex);
            app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_sb_submit_failed_unknown_error", 0, ex.getMessage()));
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "failed to submit segments", ex);
        } finally {
            lastFetchedStats = null;
        }
    }
    public static void sendSegmentSkippedViewedRequest(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        try {
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.VIEWED_SEGMENT, segment.UUID);
            final int responseCode = connection.getResponseCode();
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Successfully sent view count for segment: " + segment));
            } else {
                app.revanced.extension.shared.Logger.printDebug(() -> ((("Failed to sent view count for segment: " + segment.UUID) + " responseCode: ") + responseCode));
            }
        } catch (java.io.IOException ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "Failed to send view count", ex);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "Failed to send view count request", ex);
        }
    }
    public static void voteForSegmentOnBackgroundThread(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment, app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote voteOption) {
        app.revanced.extension.youtube.sponsorblock.requests.SBRequester.voteOrRequestCategoryChange(segment, voteOption, null);
    }
    public static void voteToChangeCategoryOnBackgroundThread(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment, app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory categoryToVoteFor) {
        app.revanced.extension.youtube.sponsorblock.requests.SBRequester.voteOrRequestCategoryChange(segment, app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote.CATEGORY_CHANGE, categoryToVoteFor);
    }
    private static void voteOrRequestCategoryChange(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment segment, app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote voteOption, app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory categoryToVoteFor) {
        app.revanced.extension.shared.Utils.runOnBackgroundThread(() -> {
    try {
        java.lang.String segmentUuid = segment.UUID;
        java.lang.String uuid = app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID();
        java.net.HttpURLConnection connection = (((voteOption == app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote.CATEGORY_CHANGE)) ? app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.VOTE_ON_SEGMENT_CATEGORY, uuid, segmentUuid, categoryToVoteFor.keyValue) : app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.VOTE_ON_SEGMENT_QUALITY, uuid, segmentUuid, java.lang.String.valueOf(voteOption.apiVoteType)));
        final int responseCode = connection.getResponseCode();
        java.lang.String userMessage;
        switch ((responseCode)) {
            case HTTP_STATUS_CODE_SUCCESS:
                app.revanced.extension.shared.Logger.printDebug(() -> ("Vote success for segment: " + segment));
                return;

            case 403:
                userMessage = app.revanced.extension.shared.StringRef.str("revanced_sb_vote_failed_forbidden", app.revanced.extension.shared.requests.Requester.parseErrorStringAndDisconnect(connection));
                break;

            default:
                userMessage = app.revanced.extension.shared.StringRef.str("revanced_sb_vote_failed_unknown_error", responseCode, connection.getResponseMessage());
                break;

        }
        app.revanced.extension.youtube.sponsorblock.SponsorBlockUtils.showErrorDialog(userMessage);
    } catch (java.net.SocketTimeoutException ex) {
        app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_vote_failed_timeout"));
    } catch (java.io.IOException ex) {
        app.revanced.extension.shared.Utils.showToastShort(app.revanced.extension.shared.StringRef.str("revanced_sb_vote_failed_unknown_error", 0, ex.getMessage()));
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "failed to vote for segment", ex);
    }
});
    }
    public static app.revanced.extension.youtube.sponsorblock.objects.UserStats retrieveUserStats() {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        try {
            app.revanced.extension.youtube.sponsorblock.objects.UserStats stats = lastFetchedStats;
            if ((((stats != null) && (!stats.isExpired())))) {
                return stats;
            }
            java.lang.String privateUserID = app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID();
            app.revanced.extension.youtube.sponsorblock.objects.UserStats fetchedStats = new app.revanced.extension.youtube.sponsorblock.objects.UserStats(privateUserID, app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getJSONObject(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.GET_USER_STATS, privateUserID));
            app.revanced.extension.shared.Logger.printDebug(() -> ("user stats: " + fetchedStats));
            lastFetchedStats = fetchedStats;
            return fetchedStats;
        } catch (java.io.IOException ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "failed to retrieve user stats", ex);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "failure retrieving user stats", ex);
        }
        return null;
    }
    public static java.lang.String setUsername(java.lang.String username) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        try {
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.CHANGE_USERNAME, app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID(), username);
            final int responseCode = connection.getResponseCode();
            java.lang.String responseMessage = connection.getResponseMessage();
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                return null;
            }
            return app.revanced.extension.shared.StringRef.str("revanced_sb_stats_username_change_unknown_error", responseCode, responseMessage);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printInfo(() -> "failed to set username", ex);
            return app.revanced.extension.shared.StringRef.str("revanced_sb_stats_username_change_unknown_error", 0, ex.getMessage());
        }
    }
    public static void runVipCheckInBackgroundIfNeeded() {
        if (((!app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.userHasSBPrivateID()))) {
            return;
        }
        long now = rv.batch.Platform.wallMillis();
        if (((now < ((app.revanced.extension.youtube.settings.Settings.SB_LAST_VIP_CHECK.get() + java.util.concurrent.TimeUnit.DAYS.toMillis(3)))))) {
            return;
        }
        app.revanced.extension.shared.Utils.runOnBackgroundThread(() -> {
    try {
        org.json.JSONObject json = app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getJSONObject(app.revanced.extension.youtube.sponsorblock.requests.SBRoutes.IS_USER_VIP, app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID());
        boolean vip = json.getBoolean("vip");
        app.revanced.extension.youtube.settings.Settings.SB_USER_IS_VIP.save(vip);
        app.revanced.extension.youtube.settings.Settings.SB_LAST_VIP_CHECK.save(now);
    } catch (java.io.IOException ex) {
        app.revanced.extension.shared.Logger.printInfo(() -> "Failed to check VIP (network error)", ex);
    } catch (java.lang.Exception ex) {
        app.revanced.extension.shared.Logger.printException(() -> "Failed to check VIP", ex);
    }
});
    }
    private static java.net.HttpURLConnection getConnectionFromRoute(app.revanced.extension.shared.requests.Route route, java.lang.String... params) throws java.io.IOException {
        java.net.HttpURLConnection connection = app.revanced.extension.shared.requests.Requester.getConnectionFromRoute(app.revanced.extension.youtube.settings.Settings.SB_API_URL.get(), route, params);
        connection.setConnectTimeout(TIMEOUT_TCP_DEFAULT_MILLISECONDS);
        connection.setReadTimeout(TIMEOUT_HTTP_DEFAULT_MILLISECONDS);
        return connection;
    }
    private static org.json.JSONObject getJSONObject(app.revanced.extension.shared.requests.Route route, java.lang.String... params) throws java.io.IOException, org.json.JSONException {
        return app.revanced.extension.shared.requests.Requester.parseJSONObject(app.revanced.extension.youtube.sponsorblock.requests.SBRequester.getConnectionFromRoute(route, params));
    }
}
