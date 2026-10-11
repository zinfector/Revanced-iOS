package app.revanced.extension.youtube.sponsorblock.objects;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.shared.StringRef;
import java.util.Objects;
import static app.revanced.extension.shared.StringRef.sf;
import android.util.Range;
// Generated from attributed editable regions, no source-body copying.
public class SponsorSegment implements java.lang.Comparable<app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment> {
    public static enum SegmentVote {
        UPVOTE(app.revanced.extension.shared.StringRef.sf("revanced_sb_vote_upvote"), 1, false),
DOWNVOTE(app.revanced.extension.shared.StringRef.sf("revanced_sb_vote_downvote"), 0, true),
CATEGORY_CHANGE(app.revanced.extension.shared.StringRef.sf("revanced_sb_vote_category"), -1, true);
        final public static app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment.SegmentVote[] voteTypesWithoutCategoryChange = {UPVOTE, DOWNVOTE};
        final public app.revanced.extension.shared.StringRef title;
        final public int apiVoteType;
        final public boolean highlightIfVipAndVideoIsLocked;
        private SegmentVote(app.revanced.extension.shared.StringRef title, int apiVoteType, boolean highlightIfVipAndVideoIsLocked) {
            this.title = title;
            this.apiVoteType = apiVoteType;
            this.highlightIfVipAndVideoIsLocked = highlightIfVipAndVideoIsLocked;
        }
    }
    final public app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category;
    final public java.lang.String UUID;
    final public long start;
    final public long end;
    final public boolean isLocked;
    public boolean didAutoSkipped = false;
    public boolean recordedAsSkipped = false;
    public SponsorSegment(app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory category, java.lang.String UUID, long start, long end, boolean isLocked) {
        this.category = category;
        this.UUID = UUID;
        this.start = start;
        this.end = end;
        this.isLocked = isLocked;
    }
    public boolean shouldAutoSkip() {
        return (category.behaviour.skipAutomatically && (!((didAutoSkipped && (category.behaviour == app.revanced.extension.youtube.sponsorblock.objects.CategoryBehaviour.SKIP_AUTOMATICALLY_ONCE)))));
    }
    public boolean startIsNear(long videoTime, long nearThreshold) {
        return (java.lang.Math.abs((start - videoTime)) <= nearThreshold);
    }
    public boolean endIsNear(long videoTime, long nearThreshold) {
        return (java.lang.Math.abs((end - videoTime)) <= nearThreshold);
    }
    public boolean containsTime(long videoTime) {
        return ((start <= videoTime) && (videoTime < end));
    }
    public boolean containsSegment(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment other) {
        return ((start <= other.start) && (other.end <= end));
    }
    public boolean intersectsRange(android.util.Range<java.lang.Long> range) {
        return ((range.getLower() < end) && (range.getUpper() >= start));
    }
    public android.util.Range<java.lang.Long> getUndoRange() {
        final long undoStart = ((category == app.revanced.extension.youtube.sponsorblock.objects.SegmentCategory.HIGHLIGHT) ? 0 : start);
        return android.util.Range.create(undoStart, (end - 1));
    }
    public long length() {
        return (end - start);
    }
    public java.lang.String getSkipButtonText() {
        return category.getSkipButtonText(start, app.revanced.extension.youtube.patches.VideoInformation.getVideoLength()).toString();
    }
    public java.lang.String getSkippedToastText() {
        return category.getSkippedToastText(start, app.revanced.extension.youtube.patches.VideoInformation.getVideoLength()).toString();
    }
    public int compareTo(app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment o) {
        return ((start == o.start) ? java.lang.Long.compare(o.length(), length()) : java.lang.Long.compare(start, o.start));
    }
    public boolean equals(java.lang.Object o) {
        java.lang.Object __rv_pattern_input_9 = null;
        app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment __rv_pattern_9 = null;
        if (((this == o))) {
            return true;
        }
        if (((!((((__rv_pattern_input_9 = o) instanceof app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment) && ((__rv_pattern_9 = ((app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment)__rv_pattern_input_9)) != null)))))) {
            return false;
        }
        return (((java.util.Objects.equals(UUID, __rv_pattern_9.UUID) && (category == __rv_pattern_9.category)) && (start == __rv_pattern_9.start)) && (end == __rv_pattern_9.end));
    }
    public int hashCode() {
        return java.util.Objects.hashCode(UUID);
    }
    public java.lang.String toString() {
        return (((((("SponsorSegment{category=" + category) + ", start=") + start) + ", end=") + end) + '}');
    }
}
