package app.revanced.extension.youtube.patches.litho;
import static app.revanced.extension.shared.StringRef.str;
import android.app.Dialog;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import androidx.annotation.Nullable;
import java.util.List;
import app.revanced.extension.shared.patches.litho.Filter;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.Utils;
import app.revanced.extension.shared.StringTrieSearch;
import app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup;
import app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup;
import app.revanced.extension.youtube.settings.Settings;
// Generated from attributed editable regions, no source-body copying.
final public class AdsFilter extends app.revanced.extension.shared.patches.litho.Filter {
    final private static app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup fullscreenAd = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "_interstitial");
    final private static java.lang.String[] PLAYER_POPUP_AD_PANEL_IDS = {"PAproduct", "jumpahead"};
    final private static java.lang.String STORE_BANNER_DOMAIN = "gstatic.com/shopping";
    final private static boolean HIDE_END_SCREEN_STORE_BANNER = app.revanced.extension.youtube.settings.Settings.HIDE_END_SCREEN_STORE_BANNER.get();
    final private app.revanced.extension.shared.StringTrieSearch exceptions = new app.revanced.extension.shared.StringTrieSearch();
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup promotionBanner;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup promotionBannerBuffer;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup buyMovieAd;
    final private app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup buyMovieAdBuffer;
    public AdsFilter() {
        exceptions.addPatterns("home_video_with_context", "related_video_with_context", "comment_thread", "|comment.", "library_recent_shelf");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup carouselAd = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_GENERAL_ADS, "carousel_ad");
        addIdentifierCallbacks(carouselAd);
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup generalAds = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_GENERAL_ADS, "_ad_with", "_buttoned_layout", "ads_video_with_context", "banner_text_icon", "brand_video_shelf", "brand_video_singleton", "carousel_footered_layout", "carousel_headered_layout", "compact_landscape_image_layout", "composite_concurrent_carousel_layout", "full_width_portrait_image_layout", "full_width_square_image_carousel_layout", "full_width_square_image_layout", "hero_promo_image", "image_button_group_layout", "landscape_image_carousel_layout", "landscape_image_wide_button_layout", "primetime_promo", "product_details", "square_image_layout", "text_image_button_layout", "text_image_no_button_layout", "video_display_button_group_layout", "video_display_carousel_button_group_layout", "video_display_carousel_buttoned_short_dr_layout", "video_display_full_buttoned_short_dr_layout", "video_display_full_layout", "watch_metadata_app_promo", "shopping_timely_shelf.");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup movieAds = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_MOVIES_SECTION, "browsy_bar", "compact_movie", "compact_tvfilm_item", "horizontal_movie_shelf", "movie_and_show_upsell_card", "offer_module_root");
        buyMovieAd = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_MOVIES_SECTION, "video_lockup_with_attachment.e");
        buyMovieAdBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "FEstorefront");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup viewProducts = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_VIEW_PRODUCTS_BANNER, "product_item", "products_in_video", "shopping_overlay.e");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup shoppingLinks = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SHOPPING_LINKS, "shopping_description_shelf.e");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup merchandise = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_MERCHANDISE_BANNERS, "product_carousel", "shopping_carousel.e");
        promotionBanner = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_YOUTUBE_PREMIUM_PROMOTIONS, "statement_banner");
        promotionBannerBuffer = new app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup(null, "img/promos/growth/", "SPunlimited");
        final app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup selfSponsor = new app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup(app.revanced.extension.youtube.settings.Settings.HIDE_SELF_SPONSOR, "cta_shelf_card");
        addPathCallbacks(buyMovieAd, generalAds, merchandise, movieAds, promotionBanner, selfSponsor, shoppingLinks, viewProducts);
    }
    public boolean isFiltered(java.lang.String identifier, java.lang.String accessibility, java.lang.String path, byte[] buffer, app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup matchedGroup, app.revanced.extension.shared.patches.litho.Filter.FilterContentType contentType, int contentIndex) {
        if (((matchedGroup == buyMovieAd))) {
            return ((contentIndex == 0) && buyMovieAdBuffer.check(buffer).isFiltered());
        }
        if (((matchedGroup == promotionBanner))) {
            return ((contentIndex == 0) && promotionBannerBuffer.check(buffer).isFiltered());
        }
        return (!exceptions.matches(path));
    }
    public static void closeFullscreenAd(java.lang.Object customDialog, byte[] buffer) {
        java.lang.Object __rv_pattern_input_4 = null;
        android.app.Dialog __rv_pattern_4 = null;
        try {
            if (((!app.revanced.extension.youtube.settings.Settings.HIDE_FULLSCREEN_ADS.get()))) {
                return;
            }
            if (((buffer == null))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "buffer is null");
                return;
            }
            if (((fullscreenAd.check(buffer).isFiltered() && (((__rv_pattern_input_4 = customDialog) instanceof android.app.Dialog) && ((__rv_pattern_4 = ((android.app.Dialog)__rv_pattern_input_4)) != null))))) {
                app.revanced.extension.shared.Logger.printDebug(() -> "Closing fullscreen ad");
                android.view.Window window = __rv_pattern_4.getWindow();
                if (((window != null))) {
                    android.view.WindowManager.LayoutParams params = window.getAttributes();
                    params.height = 0;
                    params.width = 0;
                    window.setAttributes(params);
                    window.clearFlags(2);
                    window.setSoftInputMode(0);
                    window.getDecorView().setSystemUiVisibility(0);
                }
                __rv_pattern_4.dismiss();
            }
        } catch (java.lang.Exception ex) {
            app.revanced.extension.shared.Logger.printException(() -> "closeFullscreenAd failure", ex);
        }
    }
    public static boolean hideAds() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_GENERAL_ADS.get();
    }
    public static java.lang.String hideAds(java.lang.String osName) {
        return (app.revanced.extension.youtube.settings.Settings.HIDE_GENERAL_ADS.get() ? "Android Automotive" : osName);
    }
    public static void hideAdAttributionView(android.view.View view) {
        app.revanced.extension.shared.Utils.hideViewBy0dpUnderCondition(app.revanced.extension.youtube.settings.Settings.HIDE_GENERAL_ADS, view);
    }
    public static void hideEndScreenStoreBanner(java.util.List<java.lang.Object> elementsList, java.lang.Object protobufList) {
        if (((HIDE_END_SCREEN_STORE_BANNER && protobufList.toString().contains(STORE_BANNER_DOMAIN)))) {
            app.revanced.extension.shared.Logger.printDebug(() -> "Hiding store banner");
            return;
        }
        elementsList.add(protobufList);
    }
    public static boolean hideGetPremiumView() {
        return app.revanced.extension.youtube.settings.Settings.HIDE_YOUTUBE_PREMIUM_PROMOTIONS.get();
    }
    public static boolean hidePlayerPopupAds(java.lang.String panelId) {
        return (app.revanced.extension.youtube.settings.Settings.HIDE_PLAYER_POPUP_ADS.get() && app.revanced.extension.shared.Utils.containsAny(panelId, PLAYER_POPUP_AD_PANEL_IDS));
    }
}
