package app.revanced.extension.youtube.sponsorblock.objects;
import androidx.annotation.NonNull;
import org.json.JSONException;
import org.json.JSONObject;
import app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings;
// Generated from attributed editable regions, no source-body copying.
public class UserStats {
    final private static long STATS_EXPIRATION_MILLISECONDS = ((60 * 60) * 1000);
    final private java.lang.String privateUserID;
    final public java.lang.String publicUserID;
    final public java.lang.String userName;
    final public float reputation;
    final public int totalSegmentCountIncludingIgnored;
    final public int segmentCount;
    final public int ignoredSegmentCount;
    final public int viewCount;
    final public double minutesSaved;
    final public long fetchTime;
    public UserStats(java.lang.String privateSBID, org.json.JSONObject json) throws org.json.JSONException {
        privateUserID = privateSBID;
        publicUserID = json.getString("userID");
        userName = json.getString("userName");
        reputation = ((float)json.getDouble("reputation"));
        segmentCount = json.getInt("segmentCount");
        ignoredSegmentCount = json.getInt("ignoredSegmentCount");
        totalSegmentCountIncludingIgnored = (segmentCount + ignoredSegmentCount);
        viewCount = json.getInt("viewCount");
        minutesSaved = json.getDouble("minutesSaved");
        fetchTime = rv.batch.Platform.wallMillis();
    }
    public boolean isExpired() {
        if (((STATS_EXPIRATION_MILLISECONDS < (rv.batch.Platform.wallMillis() - fetchTime)))) {
            return true;
        }
        return ((!app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.userHasSBPrivateID()) || (!app.revanced.extension.youtube.sponsorblock.SponsorBlockSettings.getSBPrivateUserID().equals(privateUserID)));
    }
    public java.lang.String toString() {
        return (((((((((((((((("UserStats{publicUserID='" + publicUserID) + '\'') + ", userName='") + userName) + '\'') + ", reputation=") + reputation) + ", segmentCount=") + segmentCount) + ", ignoredSegmentCount=") + ignoredSegmentCount) + ", viewCount=") + viewCount) + ", minutesSaved=") + minutesSaved) + '}');
    }
}
