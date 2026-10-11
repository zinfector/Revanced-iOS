package app.revanced.extension.youtube.patches.litho;
import static app.revanced.extension.youtube.shared.NavigationBar.NavigationButton;
import android.view.View;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.patches.litho.Filter;
import app.revanced.extension.shared.patches.litho.FilterGroup.*;
import app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup;
import app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList;
import app.revanced.extension.shared.patches.litho.LithoFilterPatch;
import app.revanced.extension.shared.settings.BooleanSetting;
import com.google.android.libraries.youtube.rendering.ui.pivotbar.PivotBar;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.youtube.patches.VersionCheckPatch;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.shared.EngagementPanel;
import app.revanced.extension.youtube.shared.NavigationBar;
import app.revanced.extension.youtube.shared.PlayerType;
// Generated from attributed editable regions, no source-body copying.
final public class ShortsFilter extends app.revanced.extension.shared.patches.litho.Filter {
    final private static boolean HIDE_SHORTS_NAVIGATION_BAR = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_NAVIGATION_BAR.get();
    final private static java.lang.String COMPONENT_TYPE = "ComponentType";
    final private static java.lang.String[] REEL_ACTION_BAR_PATHS = {"reel_action_bar.", "reels_player_overlay_layout."};
    final private static java.util.Map<java.lang.Integer,app.revanced.extension.shared.settings.BooleanSetting> REEL_ACTION_BUTTONS_MAP = new java.util.HashMap<>(){
    {
        put(2, app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_COMMENTS_BUTTON);
        put(3, app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SHARE_BUTTON);
        put(4, app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_REMIX_BUTTON);
    }
};
    final private java.lang.String REEL_CHANNEL_BAR_PATH = "reel_channel_bar.e";
    final private java.lang.String REEL_METAPANEL_PATH = "reel_metapanel.e";
    final private java.lang.String REEL_PLAYER_OVERLAY_PATH = "reel_player_overlay.e";
    final private static java.util.List<java.lang.String> REEL_WATCH_FRAGMENT_INIT_PLAYBACK = java.util.Arrays.asList("r_fs", "r_ts");
    final public static int HIDDEN_NAVIGATION_BAR_VERTICAL_HEIGHT = 100;
    private static java.lang.ref.WeakReference<com.google.android.libraries.youtube.rendering.ui.pivotbar.PivotBar> pivotBarRef = new java.lang.ref.WeakReference<>(null);
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shortsCompactFeedVideo;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup shortsCompactFeedVideoBuffer;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup channelProfile;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup channelProfileShelfHeaderBuffer;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup useSoundButton;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup useSoundButtonBuffer;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup useTemplateButton;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup useTemplateButtonBuffer;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup autoDubbedLabel;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup subscribeButton;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup joinButton;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup paidPromotionLabel;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shelfHeaderIdentifier;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shelfHeaderPath;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup reelCarousel;
    final private app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList reelCarouselBuffer = new app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList();
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup suggestedAction;
    final private app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList suggestedActionsBuffer = new app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList();
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shortsActionBar;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup videoActionButton;
    final private app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList videoActionButtonBuffer = new app.revanced.extension.shared.patches.litho.FilterGroupList.ByteArrayFilterGroupList();
    public ShortsFilter() {
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shortsIdentifiers = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "shorts_shelf", "inline_shorts", "shorts_grid", "shorts_video_cell");
        channelProfile = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_CHANNEL, "shorts_pivot_item");
        channelProfileShelfHeaderBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_CHANNEL, "Shorts");
        shelfHeaderIdentifier = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "shelf_header.e");
        addIdentifierCallbacks(shortsIdentifiers, channelProfile, shelfHeaderIdentifier);
        shortsCompactFeedVideo = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "compact_video.e", "video_lockup_with_attachment.e", "video_card.e");
        shortsCompactFeedVideoBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "/frame0.jpg");
        shelfHeaderPath = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "shelf_header.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup pausedOverlayButtons = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_PAUSED_OVERLAY_BUTTONS, "shorts_paused_state");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup channelBar = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_CHANNEL_BAR, REEL_CHANNEL_BAR_PATH);
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup fullVideoLinkLabel = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_FULL_VIDEO_LINK_LABEL, "reel_multi_format_link");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup videoTitle = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_VIDEO_TITLE, "shorts_video_title_item");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup reelSoundMetadata = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SOUND_METADATA_LABEL, "reel_sound_metadata");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup soundButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SOUND_BUTTON, "reel_pivot_button");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup infoPanel = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_INFO_PANEL, "shorts_info_panel_overview");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup stickers = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_STICKERS, "stickers_layer.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup likeFountain = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_LIKE_FOUNTAIN, "like_fountain.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup likeButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_LIKE_BUTTON, "shorts_like_button.e", "reel_like_button.e", "reel_like_toggled_button.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup dislikeButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_DISLIKE_BUTTON, "shorts_dislike_button.e", "reel_dislike_button.e", "reel_dislike_toggled_button.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup previewComment = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_PREVIEW_COMMENT, "participation_bar.e");
        app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup livePreview = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_LIVE_PREVIEW, "live_preview_page_vm.e");
        autoDubbedLabel = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_AUTO_DUBBED_LABEL, "badge.e");
        joinButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_JOIN_BUTTON, "sponsor_button");
        subscribeButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SUBSCRIBE_BUTTON, "subscribe_button");
        paidPromotionLabel = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_PAID_PROMOTION_LABEL, "reel_player_disclosure.e", "shorts_disclosures.e");
        shortsActionBar = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "shorts_action_bar.e", "reel_action_bar.e");
        reelCarousel = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "reel_carousel.e");
        reelCarouselBuffer.addAll(new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_AI_BUTTON, "yt_outline_info_circle", "yt_outline_experimental_info_circle"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SOUND_METADATA_LABEL, "yt_outline_audio", "yt_outline_experimental_audio"));
        useSoundButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_USE_SOUND_BUTTON, "floating_action_button.e", REEL_METAPANEL_PATH);
        useSoundButtonBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "yt_outline_camera_");
        useTemplateButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_USE_TEMPLATE_BUTTON, REEL_METAPANEL_PATH);
        useTemplateButtonBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "yt_outline_template_add_");
        videoActionButton = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "button.e");
        suggestedAction = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(null, "suggested_action.e");
        addPathCallbacks(shortsCompactFeedVideo, shelfHeaderPath, joinButton, subscribeButton, paidPromotionLabel, livePreview, suggestedAction, pausedOverlayButtons, channelBar, infoPanel, previewComment, autoDubbedLabel, fullVideoLinkLabel, videoTitle, useSoundButton, soundButton, stickers, reelCarousel, reelSoundMetadata, likeFountain, likeButton, dislikeButton);
        if (((!app.revanced.extension.youtube.patches.VersionCheckPatch.IS_20_22_OR_GREATER))) {
            addPathCallbacks(shortsActionBar);
            videoActionButtonBuffer.addAll(new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_COMMENTS_BUTTON, "reel_comment_button", "youtube_shorts_comment_outline"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SHARE_BUTTON, "reel_share_button", "youtube_shorts_share_outline"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_REMIX_BUTTON, "reel_remix_button", "youtube_shorts_remix_outline"));
        }
        suggestedActionsBuffer.addAll(new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_PREVIEW_COMMENT, "shorts-comments-panel"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SHOP_BUTTON, "yt_outline_bag_", "yt_outline_experimental_bag_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_TAGGED_PRODUCTS, "PAproduct_listZ"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_LOCATION_LABEL, "yt_outline_location_point_", "yt_outline_experimental_location_point_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SAVE_SOUND_BUTTON, "yt_outline_bookmark_", "yt_outline_list_add_", "yt_outline_experimental_list_add_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SEARCH_SUGGESTIONS, "yt_outline_search_", "yt_outline_experimental_search_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SUPER_THANKS_BUTTON, "yt_outline_dollar_sign_heart_", "yt_outline_experimental_dollar_sign_heart_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_USE_TEMPLATE_BUTTON, "yt_outline_template_add_", "yt_outline_experimental_template_add_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_UPCOMING_BUTTON, "yt_outline_bell_", "yt_outline_experimental_bell_"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_EFFECT_BUTTON, "/arcade/effects/icons/"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_GREEN_SCREEN_BUTTON, "greenscreen_temp"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_NEW_POSTS_BUTTON, "yt_outline_box_pencil", "yt_outline_experimental_box_pencil"), new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_HASHTAG_BUTTON, "yt_outline_hashtag_", "yt_outline_experimental_hashtag_"));
    }
    private boolean isEverySuggestedActionFilterEnabled() {
        for (app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup group : suggestedActionsBuffer) {
            if (((!group.isEnabled()))) {
                return false;
            }
        }
        return true;
    }
    public boolean isFiltered(java.lang.String identifier, java.lang.String accessibility, java.lang.String path, byte[] buffer, app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup matchedGroup, app.revanced.extension.shared.patches.litho.Filter.FilterContentType contentType, int contentIndex) {
        if (((contentType == app.revanced.extension.shared.patches.litho.Filter.FilterContentType.IDENTIFIER))) {
            if (((matchedGroup == shelfHeaderIdentifier))) {
                if (((contentIndex != 0))) {
                    return false;
                }
            }
            if (((matchedGroup == channelProfile))) {
                return true;
            }
            return shouldHideShortsFeedItems();
        }
        if (((contentType == app.revanced.extension.shared.patches.litho.Filter.FilterContentType.PATH))) {
            if ((((((matchedGroup == subscribeButton) || (matchedGroup == joinButton)) || (matchedGroup == paidPromotionLabel)) || (matchedGroup == autoDubbedLabel)))) {
                return ((path.startsWith(REEL_CHANNEL_BAR_PATH) || path.startsWith(REEL_METAPANEL_PATH)) || path.startsWith(REEL_PLAYER_OVERLAY_PATH));
            }
            if (((matchedGroup == reelCarousel))) {
                return reelCarouselBuffer.check(buffer).isFiltered();
            }
            if (((matchedGroup == useSoundButton))) {
                return useSoundButtonBuffer.check(buffer).isFiltered();
            }
            if (((matchedGroup == useTemplateButton))) {
                return useTemplateButtonBuffer.check(buffer).isFiltered();
            }
            if (((matchedGroup == shortsCompactFeedVideo))) {
                return (shouldHideShortsFeedItems() && shortsCompactFeedVideoBuffer.check(buffer).isFiltered());
            }
            if (((matchedGroup == shelfHeaderPath))) {
                if (((contentIndex != 0))) {
                    return false;
                }
                if (((!channelProfileShelfHeaderBuffer.check(buffer).isFiltered()))) {
                    return false;
                }
                return shouldHideShortsFeedItems();
            }
            if (((matchedGroup == shortsActionBar))) {
                return (videoActionButton.check(path).isFiltered() && videoActionButtonBuffer.check(buffer).isFiltered());
            }
            if (((matchedGroup == suggestedAction))) {
                if ((isEverySuggestedActionFilterEnabled())) {
                    return true;
                }
                return suggestedActionsBuffer.check(buffer).isFiltered();
            }
            return true;
        }
        return false;
    }
    private boolean shouldHideShortsFeedItems() {
        final boolean hideHome = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_HOME.get();
        final boolean hideSubscriptions = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SUBSCRIPTIONS.get();
        final boolean hideSearch = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SEARCH.get();
        final boolean hideVideoDescription = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_VIDEO_DESCRIPTION.get();
        final boolean hideHistory = app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_HISTORY.get();
        if (((((((!hideHome) && (!hideSubscriptions)) && (!hideSearch)) && (!hideVideoDescription)) && (!hideHistory)))) {
            return false;
        }
        if ((((((hideHome && hideSubscriptions) && hideSearch) && hideVideoDescription) && hideHistory))) {
            return true;
        }
        if ((app.revanced.extension.youtube.shared.PlayerType.getCurrent().isMaximizedOrFullscreen())) {
            return (app.revanced.extension.youtube.shared.EngagementPanel.isDescription() ? hideVideoDescription : hideHome);
        }
        if ((app.revanced.extension.youtube.shared.NavigationBar.isSearchBarActive())) {
            return hideSearch;
        }
        if (((((!hideHome) && (!hideSubscriptions)) && (!hideHistory)))) {
            return false;
        }
        app.revanced.extension.youtube.shared.NavigationBar.NavigationButton selectedNavButton = app.revanced.extension.youtube.shared.NavigationBar.NavigationButton.getSelectedNavigationButton();
        if (((selectedNavButton == null))) {
            return hideHome;
        }
        return switch ((selectedNavButton)) {    case HOME, EXPLORE -> hideHome;
    case SUBSCRIPTIONS -> hideSubscriptions;
    case LIBRARY -> hideHistory;
    default -> false;};
    }
    public static void hideActionButtons(java.lang.StringBuilder pathBuilder, java.util.List<java.lang.Object> treeNodeResultList) {
        try {
            if (((((pathBuilder == null) || (pathBuilder.length() == 0)) || (treeNodeResultList == null)))) {
                return;
            }
            int size = treeNodeResultList.size();
            if (((size < 4))) {
                return;
            }
            java.lang.String path = pathBuilder.toString();
            if ((((!app.revanced.extension.shared.Utils.containsAny(path, REEL_ACTION_BAR_PATHS)) || (!COMPONENT_TYPE.equals(treeNodeResultList.get(0).toString()))))) {
                return;
            }
            for (int i = (size - 1); (i > -1); i--) {
                java.lang.Object treeNodeResult = treeNodeResultList.get(i);
                if (((treeNodeResult != null))) {
                    app.revanced.extension.shared.settings.BooleanSetting setting = REEL_ACTION_BUTTONS_MAP.get(i);
                    if ((((setting != null) && setting.get()))) {
                        int finalI = i;
                        app.revanced.extension.shared.Logger.printDebug(() -> ((("Hiding action button by index: " + finalI) + ", key: ") + setting.key));
                        treeNodeResultList.remove(i);
                    }
                }
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "hideActionButtons failed", ex);
        }
    }
    public static int getSoundButtonSize(int original) {
        if ((app.revanced.extension.youtube.settings.Settings.HIDE_SHORTS_SOUND_BUTTON.get())) {
            return 0;
        }
        return original;
    }
    public static void setNavigationBar(com.google.android.libraries.youtube.rendering.ui.pivotbar.PivotBar view) {
        pivotBarRef = new java.lang.ref.WeakReference<>(view);
    }
    public static void hideNavigationBar(java.lang.String tag) {
        if ((HIDE_SHORTS_NAVIGATION_BAR)) {
            if ((REEL_WATCH_FRAGMENT_INIT_PLAYBACK.contains(tag))) {
                com.google.android.libraries.youtube.rendering.ui.pivotbar.PivotBar pivotBar = pivotBarRef.get();
                if (((pivotBar == null))) {
                    return;
                }
                app.revanced.extension.shared.Logger.printDebug(() -> "Hiding navbar by setting to GONE");
                pivotBar.setVisibility(8);
            } else {
                app.revanced.extension.shared.Logger.printDebug(() -> ("Ignoring tag: " + tag));
            }
        }
    }
    public static int getNavigationBarHeight(int original) {
        if ((HIDE_SHORTS_NAVIGATION_BAR)) {
            return HIDDEN_NAVIGATION_BAR_VERTICAL_HEIGHT;
        }
        return original;
    }
}
