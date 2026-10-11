package app.revanced.extension.youtube.returnyoutubedislike.requests;
import static app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.json.JSONException;
import org.json.JSONObject;
import app.revanced.extension.shared.Logger;
// Generated from attributed editable regions, no source-body copying.
final public class RYDVoteData {
    final public java.lang.String videoId;
    final public long viewCount;
    final private long fetchedLikeCount;
    private volatile long likeCount;
    final private java.lang.Long fetchedRawLikeCount;
    private volatile float likePercentage;
    final private long fetchedDislikeCount;
    private volatile long dislikeCount;
    final private java.lang.Long fetchedRawDislikeCount;
    private volatile float dislikePercentage;
    private static java.lang.Long getLongIfExist(org.json.JSONObject json, java.lang.String key) throws org.json.JSONException {
        return (json.isNull(key) ? null : json.getLong(key));
    }
    public RYDVoteData(org.json.JSONObject json) throws org.json.JSONException {
        videoId = json.getString("id");
        viewCount = json.getLong("viewCount");
        fetchedLikeCount = json.getLong("likes");
        fetchedRawLikeCount = app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData.getLongIfExist(json, "rawLikes");
        fetchedDislikeCount = json.getLong("dislikes");
        fetchedRawDislikeCount = app.revanced.extension.youtube.returnyoutubedislike.requests.RYDVoteData.getLongIfExist(json, "rawDislikes");
        if (((((viewCount < 0) || (fetchedLikeCount < 0)) || (fetchedDislikeCount < 0)))) {
            throw new org.json.JSONException(("Unexpected JSON values: " + json));
        }
        likeCount = fetchedLikeCount;
        dislikeCount = fetchedDislikeCount;
        updateUsingVote(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote.LIKE_REMOVE);
    }
    public long getLikeCount() {
        return likeCount;
    }
    public long getDislikeCount() {
        return dislikeCount;
    }
    public float getLikePercentage() {
        return likePercentage;
    }
    public float getDislikePercentage() {
        return dislikePercentage;
    }
    public void updateUsingVote(app.revanced.extension.youtube.returnyoutubedislike.ReturnYouTubeDislike.Vote vote) {
        final int likesToAdd;
        final int dislikesToAdd;
        switch ((vote)) {
            case LIKE:
                likesToAdd = 1;
                dislikesToAdd = 0;
                break;

            case DISLIKE:
                likesToAdd = 0;
                dislikesToAdd = 1;
                break;

            case LIKE_REMOVE:
                likesToAdd = 0;
                dislikesToAdd = 0;
                break;

            default:
                throw new java.lang.IllegalStateException();

        }
        final boolean videoHasNoPublicLikes = (fetchedLikeCount == 0);
        final boolean hasRawData = ((fetchedRawLikeCount != null) && (fetchedRawDislikeCount != null));
        if ((((videoHasNoPublicLikes && hasRawData) && (fetchedRawDislikeCount > 0)))) {
            final float estimatedRawScaleFactor = (((float)fetchedDislikeCount) / fetchedRawDislikeCount);
            likeCount = (((long)((estimatedRawScaleFactor * fetchedRawLikeCount))) + likesToAdd);
            app.revanced.extension.shared.Logger.printDebug(() -> "Using locally calculated estimated likes since RYD did not return an estimate");
        } else {
            likeCount = (fetchedLikeCount + likesToAdd);
        }
        dislikeCount = (fetchedDislikeCount + dislikesToAdd);
        final float totalCount = (likeCount + dislikeCount);
        if (((totalCount == 0))) {
            likePercentage = 0;
            dislikePercentage = 0;
        } else {
            likePercentage = (likeCount / totalCount);
            dislikePercentage = (dislikeCount / totalCount);
        }
    }
    public java.lang.String toString() {
        return (((((((((((("RYDVoteData{videoId=" + videoId) + ", viewCount=") + viewCount) + ", likeCount=") + likeCount) + ", dislikeCount=") + dislikeCount) + ", likePercentage=") + likePercentage) + ", dislikePercentage=") + dislikePercentage) + '}');
    }
}
