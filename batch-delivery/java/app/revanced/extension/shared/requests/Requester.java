package app.revanced.extension.shared.requests;
import app.revanced.extension.shared.Utils;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
// Generated from attributed editable regions, no source-body copying.
public class Requester {
    private Requester() {
    }
    public static java.net.HttpURLConnection getConnectionFromRoute(java.lang.String apiUrl, app.revanced.extension.shared.requests.Route route, java.lang.String... params) throws java.io.IOException {
        return app.revanced.extension.shared.requests.Requester.getConnectionFromCompiledRoute(apiUrl, route.compile(params));
    }
    public static java.net.HttpURLConnection getConnectionFromCompiledRoute(java.lang.String apiUrl, app.revanced.extension.shared.requests.Route.CompiledRoute route) throws java.io.IOException {
        java.lang.String url = (apiUrl + route.getCompiledRoute());
        java.net.HttpURLConnection connection = ((java.net.HttpURLConnection)rv.batch.Network.open(new java.net.URL(url)));
        connection.setFixedLengthStreamingMode(0);
        connection.setRequestMethod(route.getMethod().name());
        java.lang.String agentString = (((((java.lang.System.getProperty("http.agent") + "; ReVanced/") + app.revanced.extension.shared.Utils.getAppVersionName()) + " (") + app.revanced.extension.shared.Utils.getPatchesReleaseVersion()) + ")");
        connection.setRequestProperty("User-Agent", agentString);
        return connection;
    }
    private static java.lang.String parseInputStreamAndClose(java.io.InputStream inputStream) throws java.io.IOException {
        try (final java.io.BufferedReader reader = new java.io.BufferedReader(new java.io.InputStreamReader(inputStream))) {
            java.lang.StringBuilder jsonBuilder = new java.lang.StringBuilder();
            java.lang.String line;
            while ((((line = reader.readLine()) != null))) {
                jsonBuilder.append(line);
                jsonBuilder.append('\n');
            }
            return jsonBuilder.toString();
        }
    }
    public static java.lang.String parseString(java.net.HttpURLConnection connection) throws java.io.IOException {
        return app.revanced.extension.shared.requests.Requester.parseInputStreamAndClose(connection.getInputStream());
    }
    public static java.lang.String parseStringAndDisconnect(java.net.HttpURLConnection connection) throws java.io.IOException {
        java.lang.String result = app.revanced.extension.shared.requests.Requester.parseString(connection);
        connection.disconnect();
        return result;
    }
    public static java.lang.String parseErrorString(java.net.HttpURLConnection connection) throws java.io.IOException {
        java.io.InputStream errorStream = connection.getErrorStream();
        if (((errorStream == null))) {
            return "";
        }
        return app.revanced.extension.shared.requests.Requester.parseInputStreamAndClose(errorStream);
    }
    public static java.lang.String parseErrorStringAndDisconnect(java.net.HttpURLConnection connection) throws java.io.IOException {
        java.lang.String result = app.revanced.extension.shared.requests.Requester.parseErrorString(connection);
        connection.disconnect();
        return result;
    }
    public static org.json.JSONObject parseJSONObject(java.net.HttpURLConnection connection) throws org.json.JSONException, java.io.IOException {
        return new org.json.JSONObject(app.revanced.extension.shared.requests.Requester.parseString(connection));
    }
    public static org.json.JSONObject parseJSONObjectAndDisconnect(java.net.HttpURLConnection connection) throws org.json.JSONException, java.io.IOException {
        org.json.JSONObject object = app.revanced.extension.shared.requests.Requester.parseJSONObject(connection);
        connection.disconnect();
        return object;
    }
    public static org.json.JSONArray parseJSONArray(java.net.HttpURLConnection connection) throws org.json.JSONException, java.io.IOException {
        return new org.json.JSONArray(app.revanced.extension.shared.requests.Requester.parseString(connection));
    }
    public static org.json.JSONArray parseJSONArrayAndDisconnect(java.net.HttpURLConnection connection) throws org.json.JSONException, java.io.IOException {
        org.json.JSONArray array = app.revanced.extension.shared.requests.Requester.parseJSONArray(connection);
        connection.disconnect();
        return array;
    }
}
