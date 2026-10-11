package app.revanced.extension.youtube.returnyoutubedislike.requests;
import static app.revanced.extension.shared.requests.Route.Method.GET;
import static app.revanced.extension.shared.requests.Route.Method.POST;
import java.io.IOException;
import java.net.HttpURLConnection;
import app.revanced.extension.shared.requests.Requester;
import app.revanced.extension.shared.requests.Route;
// Generated from attributed editable regions, no source-body copying.
 class ReturnYouTubeDislikeRoutes {
    final static java.lang.String RYD_API_URL = "https://returnyoutubedislikeapi.com/";
    final static app.revanced.extension.shared.requests.Route SEND_VOTE = new app.revanced.extension.shared.requests.Route(POST, "interact/vote");
    final static app.revanced.extension.shared.requests.Route CONFIRM_VOTE = new app.revanced.extension.shared.requests.Route(POST, "interact/confirmVote");
    final static app.revanced.extension.shared.requests.Route GET_DISLIKES = new app.revanced.extension.shared.requests.Route(GET, "votes?videoId={video_id}");
    final static app.revanced.extension.shared.requests.Route GET_REGISTRATION = new app.revanced.extension.shared.requests.Route(GET, "puzzle/registration?userId={user_id}");
    final static app.revanced.extension.shared.requests.Route CONFIRM_REGISTRATION = new app.revanced.extension.shared.requests.Route(POST, "puzzle/registration?userId={user_id}");
    private ReturnYouTubeDislikeRoutes() {
    }
    static java.net.HttpURLConnection getRYDConnectionFromRoute(app.revanced.extension.shared.requests.Route route, java.lang.String... params) throws java.io.IOException {
        return app.revanced.extension.shared.requests.Requester.getConnectionFromRoute(RYD_API_URL, route, params);
    }
}
