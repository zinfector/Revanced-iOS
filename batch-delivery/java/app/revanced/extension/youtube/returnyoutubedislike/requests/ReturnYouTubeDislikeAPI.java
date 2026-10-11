package app.revanced.extension.youtube.returnyoutubedislike.requests;
import static app.revanced.extension.shared.StringRef.str;
import static app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute;
import android.util.Base64;
import android.widget.Toast;
import androidx.annotation.Nullable;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.IOException;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.ProtocolException;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.util.Objects;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.requests.Requester;
import app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
public class ReturnYouTubeDislikeAPI {
    final private static int API_GET_VOTES_TCP_TIMEOUT_MILLISECONDS = (3 * 1000);
    final private static int API_GET_VOTES_HTTP_TIMEOUT_MILLISECONDS = (7 * 1000);
    final private static int API_REGISTER_VOTE_TIMEOUT_MILLISECONDS = (60 * 1000);
    final private static int HTTP_STATUS_CODE_SUCCESS = 200;
    final private static int HTTP_STATUS_CODE_UNAUTHORIZED = 401;
    final private static int HTTP_STATUS_CODE_RATE_LIMIT = 429;
    final private static int BACKOFF_RATE_LIMIT_MILLISECONDS = ((10 * 60) * 1000);
    final private static int BACKOFF_CONNECTION_ERROR_MILLISECONDS = ((2 * 60) * 1000);
    private static volatile long timeToResumeAPICalls;
    private static volatile boolean lastApiCallFailed;
    private static volatile int numberOfRateLimitRequestsEncountered;
    private static volatile int fetchCallCount;
    private static volatile int fetchCallNumberOfFailures;
    private static volatile long fetchCallResponseTimeTotal;
    private static volatile long fetchCallResponseTimeLast;
    private static volatile long fetchCallResponseTimeMin;
    private static volatile long fetchCallResponseTimeMax;
    final public static int FETCH_CALL_RESPONSE_TIME_VALUE_RATE_LIMIT = -1;
    public static long getFetchCallResponseTimeLast() {
        return fetchCallResponseTimeLast;
    }
    public static long getFetchCallResponseTimeMin() {
        return fetchCallResponseTimeMin;
    }
    public static long getFetchCallResponseTimeMax() {
        return fetchCallResponseTimeMax;
    }
    public static long getFetchCallResponseTimeAverage() {
        return ((fetchCallCount == 0) ? 0 : ((fetchCallResponseTimeTotal / fetchCallCount)));
    }
    public static int getFetchCallCount() {
        return fetchCallCount;
    }
    public static int getFetchCallNumberOfFailures() {
        return fetchCallNumberOfFailures;
    }
    public static int getNumberOfRateLimitRequestsEncountered() {
        return numberOfRateLimitRequestsEncountered;
    }
    private ReturnYouTubeDislikeAPI() {
    }
    private static void randomlyWaitIfLocallyDebugging() {
        final boolean DEBUG_RANDOMLY_DELAY_NETWORK_CALLS = false;
        if ((DEBUG_RANDOMLY_DELAY_NETWORK_CALLS)) {
            final long amountOfTimeToWaste = ((long)((java.lang.Math.random() * ((API_GET_VOTES_TCP_TIMEOUT_MILLISECONDS + API_GET_VOTES_HTTP_TIMEOUT_MILLISECONDS)))));
            app.revanced.extension.shared.Utils.doNothingForDuration(amountOfTimeToWaste);
        }
    }
    public static void resetRateLimits() {
        if (((lastApiCallFailed || (timeToResumeAPICalls != 0)))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Reset rate limit");
        }
        lastApiCallFailed = false;
        timeToResumeAPICalls = 0;
    }
    private static boolean checkIfRateLimitInEffect(java.lang.String apiEndPointName) {
        if (((timeToResumeAPICalls == 0))) {
            return false;
        }
        final long now = rv.batch.Platform.wallMillis();
        if (((now > timeToResumeAPICalls))) {
            timeToResumeAPICalls = 0;
            return false;
        }
        app.revanced.extension.shared.Logger.printDebug(() -> (("Ignoring api call " + apiEndPointName) + " as rate limit is in effect"));
        return true;
    }
    private static boolean checkIfRateLimitWasHit(int httpResponseCode) {
        final boolean DEBUG_RATE_LIMIT = false;
        if ((DEBUG_RATE_LIMIT)) {
            final double RANDOM_RATE_LIMIT_PERCENTAGE = 0.2d;
            if (((java.lang.Math.random() < RANDOM_RATE_LIMIT_PERCENTAGE))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Artificially triggering rate limit for debug purposes");
                httpResponseCode = HTTP_STATUS_CODE_RATE_LIMIT;
            }
        }
        return (httpResponseCode == HTTP_STATUS_CODE_RATE_LIMIT);
    }
    private static void updateRateLimitAndStats(long timeNetworkCallStarted, boolean connectionError, boolean rateLimitHit) {
        if (((connectionError && rateLimitHit))) {
            throw new java.lang.IllegalArgumentException();
        }
        final long responseTimeOfFetchCall = (rv.batch.Platform.wallMillis() - timeNetworkCallStarted);
        fetchCallResponseTimeTotal += responseTimeOfFetchCall;
        fetchCallResponseTimeMin = (((fetchCallResponseTimeMin == 0)) ? responseTimeOfFetchCall : java.lang.Math.min(responseTimeOfFetchCall, fetchCallResponseTimeMin));
        fetchCallResponseTimeMax = java.lang.Math.max(responseTimeOfFetchCall, fetchCallResponseTimeMax);
        fetchCallCount++;
        if ((connectionError)) {
            timeToResumeAPICalls = (rv.batch.Platform.wallMillis() + BACKOFF_CONNECTION_ERROR_MILLISECONDS);
            fetchCallResponseTimeLast = responseTimeOfFetchCall;
            fetchCallNumberOfFailures++;
            lastApiCallFailed = true;
        } else {
            if ((rateLimitHit)) {
                app.revanced.extension.shared.Logger.printDebug(() -> (("API rate limit was hit. Stopping API calls for the next " + BACKOFF_RATE_LIMIT_MILLISECONDS) + " seconds"));
                timeToResumeAPICalls = (rv.batch.Platform.wallMillis() + BACKOFF_RATE_LIMIT_MILLISECONDS);
                numberOfRateLimitRequestsEncountered++;
                fetchCallResponseTimeLast = FETCH_CALL_RESPONSE_TIME_VALUE_RATE_LIMIT;
                if ((((!lastApiCallFailed) && app.revanced.extension.youtube.settings.Settings.RYD_TOAST_ON_CONNECTION_ERROR.get()))) {
                    app.revanced.extension.shared.Utils.showToastLong(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_client_rate_limit_requested"));
                }
                lastApiCallFailed = true;
            } else {
                fetchCallResponseTimeLast = responseTimeOfFetchCall;
                lastApiCallFailed = false;
            }
        }
    }
    public static void handleConnectionError(java.lang.String toastMessage, java.lang.Integer responseCode, java.lang.Exception ex, java.lang.Integer toastDuration) {
        if ((((!lastApiCallFailed) && app.revanced.extension.youtube.settings.Settings.RYD_TOAST_ON_CONNECTION_ERROR.get()))) {
            if ((((responseCode != null) && (responseCode == HTTP_STATUS_CODE_UNAUTHORIZED)))) {
                app.revanced.extension.shared.Logger.printInfo(() -> (("Ignoring status code " + HTTP_STATUS_CODE_UNAUTHORIZED) + " (API authorization error)"));
                return;
            } else {
                if (((toastDuration != null))) {
                    app.revanced.extension.shared.Utils.showToast(toastMessage, toastDuration);
                }
            }
        }
        lastApiCallFailed = true;
        app.revanced.extension.shared.Logger.printInfo(() -> toastMessage, ex);
    }
    public static app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData fetchVotes(java.lang.String videoId) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.util.Objects.requireNonNull(videoId);
        if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitInEffect("fetchVotes"))) {
            return null;
        }
        app.revanced.extension.shared.Logger.printDebug(() -> ("Fetching votes for: " + videoId));
        final long timeNetworkCallStarted = rv.batch.Platform.wallMillis();
        try {
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute(app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.GET_DISLIKES, videoId);
            connection.setRequestProperty("Accept", "application/json");
            connection.setRequestProperty("Connection", "keep-alive");
            connection.setRequestProperty("Pragma", "no-cache");
            connection.setRequestProperty("Cache-Control", "no-cache");
            connection.setUseCaches(false);
            connection.setConnectTimeout(API_GET_VOTES_TCP_TIMEOUT_MILLISECONDS);
            connection.setReadTimeout(API_GET_VOTES_HTTP_TIMEOUT_MILLISECONDS);
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.randomlyWaitIfLocallyDebugging();
            final int responseCode = connection.getResponseCode();
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitWasHit(responseCode))) {
                connection.disconnect();
                app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.updateRateLimitAndStats(timeNetworkCallStarted, false, true);
                return null;
            }
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                org.json.JSONObject json = app.revanced.extension.shared.requests.Requester.parseJSONObject(connection);
                try {
                    app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData votingData = new app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData(json);
                    app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.updateRateLimitAndStats(timeNetworkCallStarted, false, false);
                    app.revanced.extension.shared.Logger.printDebug(() -> ("Voting data fetched: " + votingData));
                    return votingData;
                } catch (org.json.JSONException ex) {
                    app.revanced.extension.shared.Logger.printException(() -> ((("Failed to parse video: " + videoId) + " JSON: ") + json), ex);
                }
            } else {
                app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_status_code", responseCode), responseCode, null, 1);
            }
            connection.disconnect();
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError((app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout")), null, ex, 0);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError((app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_generic", ex.getMessage())), null, ex, 1);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "fetchVotes failure", ex);
        }
        app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.updateRateLimitAndStats(timeNetworkCallStarted, true, false);
        return null;
    }
    public static java.lang.String registerAsNewUser() {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        try {
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitInEffect("registerAsNewUser"))) {
                return null;
            }
            java.lang.String userID = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.randomString(36);
            app.revanced.extension.shared.Logger.printDebug(() -> "Trying to register new user");
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute(app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.GET_REGISTRATION, userID);
            connection.setRequestProperty("Accept", "application/json");
            connection.setConnectTimeout(API_REGISTER_VOTE_TIMEOUT_MILLISECONDS);
            connection.setReadTimeout(API_REGISTER_VOTE_TIMEOUT_MILLISECONDS);
            final int responseCode = connection.getResponseCode();
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitWasHit(responseCode))) {
                connection.disconnect();
                return null;
            }
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                org.json.JSONObject json = app.revanced.extension.shared.requests.Requester.parseJSONObject(connection);
                java.lang.String challenge = json.getString("challenge");
                int difficulty = json.getInt("difficulty");
                java.lang.String solution = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.solvePuzzle(challenge, difficulty);
                return app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.confirmRegistration(userID, solution);
            }
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_status_code", responseCode), responseCode, null, 1);
            connection.disconnect();
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout"), null, ex, 0);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_generic", "registration failed"), null, ex, 1);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "Failed to register user", ex);
        }
        return null;
    }
    private static java.lang.String confirmRegistration(java.lang.String userID, java.lang.String solution) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.util.Objects.requireNonNull(userID);
        java.util.Objects.requireNonNull(solution);
        try {
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitInEffect("confirmRegistration"))) {
                return null;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ("Trying to confirm registration with solution: " + solution));
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute(app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.CONFIRM_REGISTRATION, userID);
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.applyCommonPostRequestSettings(connection);
            java.lang.String jsonInputString = (("{\"solution\": \"" + solution) + "\"}");
            byte[] body = jsonInputString.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(body.length);
            try (final java.io.OutputStream os = connection.getOutputStream()) {
                os.write(body);
            }
            final int responseCode = connection.getResponseCode();
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitWasHit(responseCode))) {
                connection.disconnect();
                return null;
            }
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Registration confirmation successful");
                return userID;
            }
            java.lang.String response = app.revanced.extension.shared.requests.Requester.parseStringAndDisconnect(connection);
            app.revanced.extension.shared.Logger.printInfo(() -> (((((((("Failed to confirm registration for user: " + userID) + " solution: ") + solution) + " responseCode: ") + responseCode) + " response: '") + response) + "''"));
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_status_code", responseCode), responseCode, null, 1);
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout"), null, ex, 0);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_generic", "confirm registration failed"), null, ex, 1);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> ((("Failed to confirm registration for user: " + userID) + "solution: ") + solution), ex);
        }
        return null;
    }
    private static java.lang.String getUserID() {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.lang.String userID = app.revanced.extension.youtube.settings.Settings.RYD_USER_ID.get();
        if (((!userID.isEmpty()))) {
            return userID;
        }
        userID = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.registerAsNewUser();
        if (((userID != null))) {
            app.revanced.extension.youtube.settings.Settings.RYD_USER_ID.save(userID);
        }
        return userID;
    }
    public static boolean sendVote(java.lang.String videoId, app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote vote) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.util.Objects.requireNonNull(videoId);
        java.util.Objects.requireNonNull(vote);
        try {
            java.lang.String userID = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.getUserID();
            if (((userID == null))) {
                return false;
            }
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitInEffect("sendVote"))) {
                return false;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ((("Trying to vote for video: " + videoId) + " with vote: ") + vote));
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute(app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.SEND_VOTE);
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.applyCommonPostRequestSettings(connection);
            java.lang.String voteJsonString = (((((("{\"userId\": \"" + userID) + "\", \"videoId\": \"") + videoId) + "\", \"value\": \"") + vote.value) + "\"}");
            byte[] body = voteJsonString.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(body.length);
            try (final java.io.OutputStream os = connection.getOutputStream()) {
                os.write(body);
            }
            final int responseCode = connection.getResponseCode();
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitWasHit(responseCode))) {
                connection.disconnect();
                return false;
            }
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                org.json.JSONObject json = app.revanced.extension.shared.requests.Requester.parseJSONObject(connection);
                java.lang.String challenge = json.getString("challenge");
                int difficulty = json.getInt("difficulty");
                java.lang.String solution = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.solvePuzzle(challenge, difficulty);
                return app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.confirmVote(videoId, userID, solution);
            }
            app.revanced.extension.shared.Logger.printInfo(() -> ((((("Failed to send vote for video: " + videoId) + " vote: ") + vote) + " response code was: ") + responseCode));
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_status_code", responseCode), responseCode, null, 1);
            connection.disconnect();
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout"), null, ex, 0);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_generic", "send vote failed"), null, ex, 1);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> ((("Failed to send vote for video: " + videoId) + " vote: ") + vote), ex);
        }
        return false;
    }
    private static boolean confirmVote(java.lang.String videoId, java.lang.String userID, java.lang.String solution) {
        app.revanced.extension.shared.Utils.verifyOffMainThread();
        java.util.Objects.requireNonNull(videoId);
        java.util.Objects.requireNonNull(userID);
        java.util.Objects.requireNonNull(solution);
        try {
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitInEffect("confirmVote"))) {
                return false;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ((("Trying to confirm vote for video: " + videoId) + " solution: ") + solution));
            java.net.HttpURLConnection connection = app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.getRYDConnectionFromRoute(app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeRoutes.CONFIRM_VOTE);
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.applyCommonPostRequestSettings(connection);
            java.lang.String jsonInputString = (((((("{\"userId\": \"" + userID) + "\", \"videoId\": \"") + videoId) + "\", \"solution\": \"") + solution) + "\"}");
            byte[] body = jsonInputString.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(body.length);
            try (final java.io.OutputStream os = connection.getOutputStream()) {
                os.write(body);
            }
            final int responseCode = connection.getResponseCode();
            if ((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.checkIfRateLimitWasHit(responseCode))) {
                connection.disconnect();
                return false;
            }
            if (((responseCode == HTTP_STATUS_CODE_SUCCESS))) {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Vote confirm successful for video: " + videoId));
                return true;
            }
            java.lang.String response = app.revanced.extension.shared.requests.Requester.parseStringAndDisconnect(connection);
            app.revanced.extension.shared.Logger.printInfo(() -> (((((((("Failed to confirm vote for video: " + videoId) + " solution: ") + solution) + " responseCode: ") + responseCode) + " response: '") + response) + "'"));
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_status_code", responseCode), responseCode, null, 1);
        } catch (java.net.SocketTimeoutException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_connection_timeout"), null, ex, 0);
        } catch (java.io.IOException ex) {
            app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.handleConnectionError(app.revanced.extension.shared.StringRef.str("revanced_ryd_failure_generic", "confirm vote failed"), null, ex, 1);
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> ((("Failed to confirm vote for video: " + videoId) + " solution: ") + solution), ex);
        }
        return false;
    }
    private static void applyCommonPostRequestSettings(java.net.HttpURLConnection connection) throws java.net.ProtocolException {
        connection.setRequestMethod("POST");
        connection.setRequestProperty("Content-Type", "application/json");
        connection.setRequestProperty("Accept", "application/json");
        connection.setRequestProperty("Pragma", "no-cache");
        connection.setRequestProperty("Cache-Control", "no-cache");
        connection.setUseCaches(false);
        connection.setDoOutput(true);
        connection.setConnectTimeout(API_REGISTER_VOTE_TIMEOUT_MILLISECONDS);
        connection.setReadTimeout(API_REGISTER_VOTE_TIMEOUT_MILLISECONDS);
    }
    private static java.lang.String solvePuzzle(java.lang.String challenge, int difficulty) {
        final long timeSolveStarted = rv.batch.Platform.wallMillis();
        byte[] decodedChallenge = android.util.Base64.decode(challenge, 2);
        byte[] buffer = new byte[20];
        java.lang.System.arraycopy(decodedChallenge, 0, buffer, 4, 16);
        java.security.MessageDigest md;
        try {
            md = java.security.MessageDigest.getInstance("SHA-512");
        } catch (java.security.NoSuchAlgorithmException ex) {
            throw new java.lang.IllegalStateException(ex);
        }
        final int maxCount = ((int)((java.lang.Math.pow(2, (difficulty + 1)) * 5)));
        for (int i = 0; (i < maxCount); i++) {
            buffer[0] = ((byte)i);
            buffer[1] = ((byte)((i >> 8)));
            buffer[2] = ((byte)((i >> 16)));
            buffer[3] = ((byte)((i >> 24)));
            byte[] messageDigest = md.digest(buffer);
            if (((app.revanced.extension.youtube.returnyoutubedislike.requests.ReturnYouTubeDislikeAPI.countLeadingZeroes(messageDigest) >= difficulty))) {
                java.lang.String solution = android.util.Base64.encodeToString(new byte[] {buffer[0], buffer[1], buffer[2], buffer[3]}, 2);
                app.revanced.extension.shared.Logger.printDebug(() -> (((((("Found puzzle solution: " + solution) + " of difficulty: ") + difficulty) + " in: ") + ((rv.batch.Platform.wallMillis() - timeSolveStarted))) + " ms"));
                return solution;
            }
        }
        throw new java.lang.IllegalStateException(((("Failed to solve puzzle challenge: " + challenge) + " difficulty: ") + difficulty));
    }
    private static java.lang.String randomString(int len) {
        java.lang.String AB = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        java.security.SecureRandom rnd = new java.security.SecureRandom();
        java.lang.StringBuilder sb = new java.lang.StringBuilder(len);
        for (int i = 0; (i < len); i++) {
            sb.append(AB.charAt(rnd.nextInt(AB.length())));
        }
        return sb.toString();
    }
    private static int countLeadingZeroes(byte[] uInt8View) {
        int zeroes = 0;
        for (byte b : uInt8View) {
            int value = (b & 255);
            if (((value == 0))) {
                zeroes += 8;
            } else {
                int count = 1;
                if ((((value >>> 4) == 0))) {
                    count += 4;
                    value <<= 4;
                }
                if ((((value >>> 6) == 0))) {
                    count += 2;
                    value <<= 2;
                }
                zeroes += (count - ((value >>> 7)));
                break;
            }
        }
        return zeroes;
    }
}
