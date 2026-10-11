package app.revanced.extension.youtube.sponsorblock.requests;
import static app.revanced.extension.shared.requests.Route.Method.GET;
import static app.revanced.extension.shared.requests.Route.Method.POST;
import app.revanced.extension.shared.requests.Route;
// Generated from attributed editable regions, no source-body copying.
 class SBRoutes {
    final static app.revanced.extension.shared.requests.Route IS_USER_VIP = new app.revanced.extension.shared.requests.Route(GET, "/api/isUserVIP?userID={user_id}");
    final static app.revanced.extension.shared.requests.Route GET_SEGMENTS = new app.revanced.extension.shared.requests.Route(GET, "/api/skipSegments?videoID={video_id}&categories={categories}");
    final static app.revanced.extension.shared.requests.Route VIEWED_SEGMENT = new app.revanced.extension.shared.requests.Route(POST, "/api/viewedVideoSponsorTime?UUID={segment_id}");
    final static app.revanced.extension.shared.requests.Route GET_USER_STATS = new app.revanced.extension.shared.requests.Route(GET, "/api/userInfo?userID={user_id}&values=[\"userID\",\"userName\",\"reputation\",\"segmentCount\",\"ignoredSegmentCount\",\"viewCount\",\"minutesSaved\"]");
    final static app.revanced.extension.shared.requests.Route CHANGE_USERNAME = new app.revanced.extension.shared.requests.Route(POST, "/api/setUsername?userID={user_id}&username={username}");
    final static app.revanced.extension.shared.requests.Route SUBMIT_SEGMENTS = new app.revanced.extension.shared.requests.Route(POST, "/api/skipSegments?userID={user_id}&videoID={video_id}&category={category}&startTime={start_time}&endTime={end_time}&videoDuration={duration}");
    final static app.revanced.extension.shared.requests.Route VOTE_ON_SEGMENT_QUALITY = new app.revanced.extension.shared.requests.Route(POST, "/api/voteOnSponsorTime?userID={user_id}&UUID={segment_id}&type={type}");
    final static app.revanced.extension.shared.requests.Route VOTE_ON_SEGMENT_CATEGORY = new app.revanced.extension.shared.requests.Route(POST, "/api/voteOnSponsorTime?userID={user_id}&UUID={segment_id}&category={category}");
    private SBRoutes() {
    }
}
