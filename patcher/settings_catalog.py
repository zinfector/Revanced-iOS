"""Preference groups and validation rules shared by the native ReVanced settings UI."""
import json
from features import CATALOG

GROUPS = [
    ('ads','Ads','Feed ads also controls sponsored cards and below-video banners. Reopen the affected screen after changing filters.', 'video_ads feed_ads shorts_ads'),
    ('player','Player','Playback changes apply when the next video is opened.', 'background_playback picture_in_picture loop_video pause_on_interrupt disable_auto_captions force_original_audio open_videos_fullscreen exit_fullscreen_end custom_speed_menu remember_speed'),
    ('video','Video quality','Resolution caps filter available formats. Reopen the video to apply them.', 'advanced_quality_menu hide_premium_quality remember_quality disable_hdr disable_vp9'),
    ('shorts','Shorts','The app-icon shortcut can be cached by iOS.', 'hide_shorts disable_shorts_resume shorts_autoplay open_shorts_regular hide_shorts_shortcut'),
    ('navigation','Navigation','Tab changes apply immediately. At least one content tab is kept available. The start page applies when the app opens.', 'hide_home_navigation hide_shorts_navigation hide_subscriptions_navigation hide_library_navigation hide_navigation_labels'),
    ('miniplayer','Miniplayer','Reopen the miniplayer after changing size or layout. Background opacity preserves native fades. Hiding overlay buttons retains video taps and ad-skip controls.', 'classic_miniplayer miniplayer_disable_drag miniplayer_disable_horizontal_drag miniplayer_disable_double_tap miniplayer_hide_subtext miniplayer_square_corners miniplayer_hide_overlay_buttons'),
    ('seekbar','Seekbar and gestures','Gesture options apply to the native player. Custom swipes require fullscreen.', 'hide_timestamp hide_seekbar disable_double_tap disable_chapter_skip disable_precise_seeking tap_to_seek swipe_controls'),
    ('layout','Layout and appearance','Reopen the affected screen after changing layout or theme.', 'hide_cast_button hide_captions_button hide_autoplay_button hide_previous_next hide_watermark hide_end_cards hide_autoplay_preview hide_info_cards hide_related_overlay disable_ambient disable_popup_panels hide_action_buttons hide_flyout_items hide_comments hide_layout_components disable_haptics disable_rolling_numbers custom_header'),
    ('thumbnails','Thumbnails','Already loaded thumbnails may need a feed refresh. Per-screen modes override the global thumbnail options.', 'alternative_thumbnails fast_thumbnail_stills thumbnail_proxy'),
    ('dearrow','DeArrow','Community titles and thumbnails apply independently to ordinary videos. Unavailable replacements retain the original. Screen overrides apply immediately.', 'dearrow_titles dearrow_thumbnails'),
    ('links','Links and downloads','Downloads open a share sheet and require a compatible downloader.', 'copy_video_url external_downloads bypass_redirects open_links_external sanitize_sharing_links'),
    ('ryd','Return YouTube Dislike','Estimates and service votes are separate from YouTube account votes. Voting is available in Video tools.', 'return_dislikes paired_vote_buttons first_launch_ui ryd_voting'),
    ('sponsorblock','SponsorBlock','Reopen the video after changing categories. Votes and submissions are available in Video tools.', 'sponsorblock sponsorblock_manual sponsorblock_contribute sponsorblock_markers'),
    ('authentication','Authentication','Keep these enabled for the tested SideStore sign-in path. Restart after changing either switch.', 'sideload_auth_identity sideload_auth_keychain'),
    ('advanced','Advanced','Request overrides do not replace the media stream. Branding images and the installed app name require rebuilding the IPA.', 'spoof_app_version spoof_dimensions spoof_form_factor disable_tv_popup remove_discretion_dialog announcements watch_history_dns'),
]

PARAMETERS = {
    'ad_strategy':('ads','Video ad strategy','choice',dict(choices=['response','trigger','coordinator'],choice_labels=['Player response','Player response (trigger fallback)','Native ad coordinator'],hint='The trigger option uses response filtering. Reopen the video after changing strategy.')),
    'default_speed':('player','Default playback speed','number',dict(min=.25,max=4,options=[.25,.5,.75,1,1.25,1.5,1.75,2,2.5,3,4],unit='x')),
    'custom_speeds':('player','Playback speed choices','number_list',dict(min=.25,max=4,list_min=1,list_max=30,unit='x')),
    'default_quality':('video','Default resolution cap','choice',dict(choices=[0,144,240,360,480,720,1080,1440,2160],integer=True,choice_labels=['Auto','144p','240p','360p','480p','720p','1080p','1440p','2160p'])),
    'wifi_quality':('video','Wi-Fi resolution cap','choice',dict(choices=[-1,0,144,240,360,480,720,1080,1440,2160],integer=True,choice_labels=['Use default','Auto','144p','240p','360p','480p','720p','1080p','1440p','2160p'])),
    'cellular_quality':('video','Cellular resolution cap','choice',dict(choices=[-1,0,144,240,360,480,720,1080,1440,2160],integer=True,choice_labels=['Use default','Auto','144p','240p','360p','480p','720p','1080p','1440p','2160p'])),
    'miniplayer_min_dimension_points':('miniplayer','Minimum miniplayer size','number',dict(min=170,max=480,allow_zero=True,options=[0,170,192,240,300,360,480],unit='points',zero_label='Native size')),
    'miniplayer_overlay_opacity':('miniplayer','Control-background opacity','number',dict(min=0,max=1,options=[0,.25,.5,.75,1])),
    'double_tap_seconds':('seekbar','Double-tap seek interval','number',dict(min=0,max=120,options=[0,5,10,15,20,30,60,120],unit='seconds',zero_label='Native interval')),
    'seekbar_color':('seekbar','Seekbar color','string',dict(pattern=r'^#[0-9A-Fa-f]{6}$',allow_empty=True,hint='Use #RRGGBB, or leave empty for the native color.')),
    'overlay_opacity':('layout','Player overlay opacity','number',dict(min=0,max=1,options=[0,.25,.5,.75,1])),
    'theme':('layout','Theme','choice',dict(choices=['system','dark','light'],choice_labels=['System','Dark','Light'])),
    'theme_light_background':('layout','Light background color','string',dict(pattern=r'^#[0-9A-Fa-f]{6}$',allow_empty=True,hint='Use #RRGGBB, or leave empty for the native color.')),
    'theme_dark_background':('layout','Dark background color','string',dict(pattern=r'^#[0-9A-Fa-f]{6}$',allow_empty=True,hint='Use #RRGGBB, or leave empty for the native color.')),
    'start_page':('navigation','Start page','choice',dict(choices=['','FEwhat_to_watch','FEsubscriptions','FElibrary','FEshorts'],choice_labels=['Native','Home','Subscriptions','You / Library','Shorts'])),
    'thumbnail_frame':('thumbnails','Video-frame thumbnail','choice',dict(choices=[1,2,3],integer=True,choice_labels=['First frame','Middle frame','Last frame'])),
    'thumbnail_modes':('thumbnails','Per-screen thumbnail modes','map',dict(map_keys=['home','subscriptions','library','player','search'],choices=['original','stills','dearrow','dearrow-stills'],choice_labels=['Original','Video frame','DeArrow','DeArrow with frame fallback'])),
    'thumbnail_proxy_url':('thumbnails','Thumbnail proxy URL','string',dict(url=True,allow_empty=True,max_length=1024,hint='An HTTPS endpoint that accepts a url query parameter. Empty disables the configured endpoint.')),
    'dearrow_url':('dearrow','Thumbnail service URL','string',dict(url=True,allow_empty=False,max_length=1024,hint='An HTTPS thumbnail endpoint.')),
    'dearrow_branding_url':('dearrow','Branding service URL','string',dict(url=True,allow_empty=False,max_length=1024,hint='The HTTPS branding-data endpoint; separate from the image service.')),
    'dearrow_title_modes':('dearrow','Titles by screen','map',dict(map_keys=['home','search','subscriptions','library','player','related'],choices=['original','custom'],choice_labels=['Original','Community title'])),
    'dearrow_thumbnail_modes':('dearrow','Thumbnails by screen','map',dict(map_keys=['home','search','subscriptions','library','related'],choices=['original','custom'],choice_labels=['Original','Community thumbnail'])),
    'dearrow_fallback':('dearrow','Thumbnail fallback','choice',dict(choices=['original','stills'],choice_labels=['Original','Available YouTube frame'],hint='A frame fallback uses only an already verified native still; otherwise retain the original.')),
    'sponsor_categories':('sponsorblock','Enabled categories','string_list',dict(allowed=['sponsor','selfpromo','interaction','intro','outro','preview','music_offtopic','filler','hook','poi_highlight'],list_min=0,list_max=64,max_length=128)),
    'sponsor_behaviors':('sponsorblock','Category actions','map',dict(map_keys=['sponsor','selfpromo','interaction','intro','outro','preview','music_offtopic','filler','hook','poi_highlight'],choices=['skip','skip-once','manual-skip','seekbar-only','ignore'],choice_labels=['Skip','Skip once','Ask to skip','Seekbar markers only','Ignore'])),
    'sponsor_colors':('sponsorblock','Category marker colors','map',dict(map_keys=['sponsor','selfpromo','interaction','intro','outro','preview','music_offtopic','filler','hook','poi_highlight'],pattern=r'^#[0-9A-Fa-f]{6}$',hint='Use #RRGGBB, or leave empty for the default color.')),
    'sponsor_min_duration':('sponsorblock','Minimum segment duration','number',dict(min=0,max=600,unit='seconds')),
    'client_version':('advanced','Request client version','string',dict(pattern=r'^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$',hint='Three numeric parts, for example 21.39.4. Requires the request-version switch.')),
    'screen_width_points':('advanced','Request screen width','number',dict(min=1,max=8192,integer=True,unit='points')),
    'screen_height_points':('advanced','Request screen height','number',dict(min=1,max=8192,integer=True,unit='points')),
    'form_factor':('advanced','Request form factor','choice',dict(choices=['phone','tablet'],choice_labels=['Phone','Tablet'])),
    'diagnostics':('advanced','Diagnostic logging','bool',dict(hint='Records hook installation and bounded, redacted authentication events.')),
}
for key,title,group in [('feed_patterns','Feed ad filters','ads'),('shorts_patterns','Shorts shelf filters','shorts'),
                        ('action_patterns','Video action filters','layout'),('flyout_patterns','Player menu filters','layout'),
                        ('comment_patterns','Comment filters','layout'),('layout_patterns','Custom component filters','layout')]:
    PARAMETERS[key]=(group,title,'string_list',dict(list_min=0,list_max=64,max_length=128,hint='One positive component pattern per line. Empty lists match nothing.'))

DESCRIPTIONS = {
    'hide_navigation_labels':'Keeps the tab icons and their accessible names.',
    'hide_library_navigation':'Hides the You tab, also called Library in some layouts.',
    'first_launch_ui':'Use YouTube default cold configuration on every launch. Affects native experiment defaults throughout the app. Keeps saved configuration on disk. Restart after changing this switch.',
    'paired_vote_buttons':'Use native paired like/dislike controls when their model is supported. Reopen the video after changing this setting.',
    'sideload_auth_identity':'Uses the original YouTube identity only in the verified native sign-in flow.',
    'sideload_auth_keychain':'Uses native private-keychain storage for the installed signing identity.',
    'custom_header':'Uses the header image supplied when this IPA was built.',
    'miniplayer_hide_subtext':'Hides messages and Premium badges; retains the separate native ad badge.',
    'miniplayer_square_corners':'Preserves native masks during collapse and expand animations.',
    'miniplayer_disable_horizontal_drag':'Retains vertical movement and dismissal.',
    'classic_miniplayer':'Disables the floating-miniplayer experiment.',
    'miniplayer_hide_overlay_buttons':'Hides floating close/playback buttons and their backgrounds. Retains video-tap expansion, progress and the separate ad-skip control.',
    'watch_history_dns':'Adds the DNS diagnostic to Video tools; does not change your DNS server.',
    'remove_discretion_dialog':'Confirms ordinary content warnings. Age, account and rental verification stays native.',
    'sponsorblock_manual':'Uses manual skipping in place of automatic category skipping.',
}

def catalog():
    from patcher import DEFAULTS
    groups=[dict(id=g,title=t,hint=h,keys=keys.split()) for g,t,h,keys in GROUPS]
    descriptors={key:dict(key=key,title=title,kind='bool',hint=DESCRIPTIONS.get(key,''),default=DEFAULTS[key]) for key,title in CATALOG.items()}
    by_id={g['id']:g for g in groups}
    for key,(group,title,kind,rules) in PARAMETERS.items():
        by_id[group]['keys'].append(key)
        descriptors[key]=dict(key=key,title=title,kind=kind,default=DEFAULTS[key],**rules)
    grouped=[key for g in groups for key in g['keys']]
    assert len(grouped)==len(set(grouped)) and set(grouped)==set(DEFAULTS)-{'schema','app_name'}
    return dict(groups=groups,settings=descriptors)

def native_header():
    value=json.dumps(catalog(),separators=(',',':'),ensure_ascii=True)
    return '// Generated from settings_catalog.py and patcher defaults.\n#define RVSettingsCatalogJSON @'+json.dumps(value,ensure_ascii=True)+'\n'
