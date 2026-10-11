package app.revanced.extension.youtube.patches.litho;
import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.LinkedHashSet;
import java.util.Map;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.TrieSearch;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.patches.litho.Filter;
import app.revanced.extension.shared.patches.litho.FilterGroup.*;
import app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList;
import app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch;
import app.revanced.extension.youtube.patches.VideoInformation;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class ReturnYouTubeDislikeFilter extends app.revanced.extension.shared.patches.litho.Filter {
    final private static java.util.Map<java.lang.String,java.lang.Boolean> lastVideoIds = app.revanced.extension.shared.Utils.createSizeRestrictedMap(5);
    public static void newPlayerResponseVideoId(java.lang.String videoId, boolean isShortAndOpeningOrPlaying) {
        try {
            if (((((!isShortAndOpeningOrPlaying) || (!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get())) || (!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get())))) {
                return;
            }
            synchronized ((lastVideoIds)) {
                if (((lastVideoIds.put(videoId, java.lang.Boolean.TRUE) == null))) {
                    app.revanced.extension.shared.Logger.printDebug(() -> ("New Short video ID: " + videoId));
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "newPlayerResponseVideoId failure", ex);
        }
    }
    final private app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList videoIdFilterGroup = new app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList();
    public ReturnYouTubeDislikeFilter() {
        addPathCallbacks(new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "shorts_like_button.e", "reel_like_button.e", "reel_like_toggled_button.e", "shorts_dislike_button.e", "reel_dislike_button.e", "reel_dislike_toggled_button.e"));
        videoIdFilterGroup.addAll(new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "id.reel_like_button", "id.reel_dislike_button", "ic_right_like", "ic_right_dislike"));
    }
    public boolean isFiltered(java.lang.String identifier, java.lang.String accessibility, java.lang.String path, byte[] buffer, app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup matchedGroup, app.revanced.extension.shared.patches.litho.Filter.FilterContentType contentType, int contentIndex) {
        if ((((!app.revanced.extension.youtube.settings.Settings.RYD_ENABLED.get()) || (!app.revanced.extension.youtube.settings.Settings.RYD_SHORTS.get())))) {
            return false;
        }
        app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult result = videoIdFilterGroup.check(buffer);
        if ((result.isFiltered())) {
            java.lang.String matchedVideoId = findVideoId(buffer);
            app.revanced.extension.youtube.patches.ReturnYouTubeDislikePatch.setLastLithoShortsVideoId(matchedVideoId);
        }
        return false;
    }
    private java.lang.String findVideoId(byte[] protobufBufferArray) {
        synchronized ((lastVideoIds)) {
            for (java.lang.String videoId : lastVideoIds.keySet()) {
                if ((app.revanced.extension.youtube.patches.litho.ReturnYouTubeDislikeFilter.byteArrayContainsString(protobufBufferArray, videoId))) {
                    return videoId;
                }
            }
            return null;
        }
    }
    private static boolean byteArrayContainsString(byte[] array, java.lang.String text) {
        for (int i = 0, lastArrayStartIndex = (array.length - text.length()); (i <= lastArrayStartIndex); i++) {
            boolean found = true;
            for (int j = 0, textLength = text.length(); (j < textLength); j++) {
                if (((array[(i + j)] != ((byte)text.charAt(j))))) {
                    found = false;
                    break;
                }
            }
            if ((found)) {
                return true;
            }
        }
        return false;
    }
}
