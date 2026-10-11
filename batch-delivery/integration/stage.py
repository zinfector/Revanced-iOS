"""Generate the app integration catalog from source settings and ABI evidence.

No legacy iOS implementation files or payloads are copied or compiled.
"""
import argparse,hashlib,json,re,shutil,sys,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parent
BATCH=ROOT.parent
PROJECT=BATCH.parents[1]
sys.path.insert(0,str(BATCH))
from prepare_cloud import prepare

def catalog():
    facts=PROJECT/'runtime-integration/cloud-build-0.4.6/patcher/profiles'
    def load(n):return json.loads((facts/n).read_text(encoding='utf-8'))
    app=load('youtube-21.39.4.json');literal=load('literal-hook-metadata.json');auth=load('authentication-evidence.json')
    headers={}
    for c in literal:
        for m in c['methods']:headers[c['name'],m['kind'],m['selector']]=m['encoding']
    for h in app['hooks']:headers[h['class'],h['kind'],h['selector']]=h['encoding']
    for name in ('native-dislikes-evidence.json','sponsorblock-evidence.json','sponsorblock-response-evidence.json'):
        data=load(name)
        for c in data.get('metadata',data.get('metadata_anchors',[])):
            for m in c['methods']:headers[c['name'],m['kind'],m['selector']]=m['encoding']
    for h in auth['native_anchors']:headers[h['class'],h['kind'],h['selector']]=h['encoding']
    hooks=[]
    def add(cls,selector,driver,types=None,kind='-',**extra):
        encoding=headers.get((cls,kind,selector))
        if not encoding and types is None:raise ValueError('Missing ABI fact: '+cls+'.'+selector)
        types=types or re.sub(r'[0-9]','',encoding)
        hooks.append(dict(**{'class':cls,'kind':kind,'selector':selector,'types':types,'driver':driver},**extra))
    for h in app['hooks']:
        if h['feature']=='background_playback' and re.sub(r'[0-9]','',h['encoding'])=='B@:':add(h['class'],h['selector'],'bool',feature='background_playback',value=True)
    for cls,sel in [('YTBackgroundabilityPolicyImpl','isPlayableInPictureInPictureByUserSettings'),('YTColdConfig','enablePipMenuItem'),('YTColdConfig','addPipMenuItem')]:add(cls,sel,'bool',feature='picture_in_picture',value=True)
    add('YTColdConfig','enableResumeToShorts','bool',feature='disable_shorts_resume',value=False)
    # Supported source placements are data; the shared host implements each ABI shape once.
    for sel,driver in [('potentiallyMutatedSingleVideo:currentVideoTimeDidChange:','clock'),('playbackController:didLoadContentPlaybackData:','loaded')]:add('YTPlayerViewController',sel,driver,'v@:@@')
    add('YTPlayerViewController','setPlaybackRate:','rate','v@:f')
    add('YTMainAppVideoPlayerOverlayViewController','didPressVarispeed:','speed_menu')
    add('YTSlimVideoDetailsActionView','layoutSubviews','layout')
    add('YTMainVideoEntityModel','dislikeCountString','dislike_text')
    for cls in ('YTSectionListViewController','YTInnerTubeCollectionViewController','YTWatchNextResultsViewController'):add(cls,'loadWithModel:','filter','v@:@')
    for cls,sel,val,kind in [('SSOBundleIdServiceImpl','bundleId',app['bundle'],'-'),('SSOConfiguration','applicationIdentifier',app['bundle'],'-'),('SSOConfiguration','applicationScheme',auth['original_callback_scheme'],'-'),('SSOConfiguration','authCallbackURLString',auth['original_callback_scheme']+':/authCallback','-'),('SSOConfiguration','clientID',auth['original_client_id'],'-'),('SSOConfiguration','libraryClientID',auth['sso_library_client_id'],'+'),('SSOKeychainHelper','accessGroup',None,'+'),('SSOKeychainHelper','sharedAccessGroup',None,'+')]:add(cls,sel,'identity',kind=kind,value=val)
    add('SSOKeychainHelper','usePrivateKeychain','identity_bool',kind='+',value=True)
    add('SSOConfiguration','shouldEnableSafariSignIn','identity_bool',value=True)
    for sel in ('queryMatchingID:serviceName:','deleteQueryMatchingID:serviceName:'):add('SSOKeychainHelper',sel,'keychain_query',kind='+')
    protobuf=load('adblock-display-protobuf-fields.json')
    messages={}
    for cls,desc in protobuf.items():
        children=[];classify=[]
        for f in desc['fields']:
            name=f['name']
            if f['message_class'] and f['message_class'] in protobuf:
                children.append(dict(getter=name,setter='set'+name[0].upper()+name[1:]+':',array=f['repeated']))
            if cls.endswith('SupportedRenderers') and f['message_class'] and not f['repeated']:
                if name.startswith('promoted') or 'Ad' in name or name.startswith('ad'):
                    classify.append(dict(presence='has'+name[0].upper()+name[1:],family='ads',semantic='carousel_ad',feature='feed_ads'))
                elif name in ('reelShelfRenderer','reelGridRenderer','reelItemRenderer'):
                    classify.append(dict(presence='has'+name[0].upper()+name[1:],family='shorts',semantic='shorts_shelf',feature='hide_shorts'))
            if cls=='YTIPlayerResponse' and name in ('playerAdsArray','adPlacementsArray','adSlotsArray'):
                add(cls,name,'ad_array','@@:');add(cls,name+'_Count','ad_count','Q@:')
        messages[cls]=dict(children=children,classify=classify)
    # GPB repeated collections have one reusable native operation, no platform offsets.
    messages.setdefault('YTIItemSectionRenderer',dict(children=[dict(getter='contentsArray',setter='setContentsArray:',array=True)],classify=[]))
    pairs={'video_ads':'revanced_hide_video_ads','feed_ads':'revanced_hide_general_ads','return_dislikes':'revanced_ryd_enabled','sponsorblock':'sb_enabled','remember_speed':'revanced_remember_playback_speed_last_selected','hide_info_cards':'revanced_hide_info_cards','hide_end_cards':'revanced_hide_end_screen_cards','hide_autoplay_preview':'revanced_hide_autoplay_preview','disable_popup_panels':'revanced_disable_player_popup_panels','disable_shorts_resume':'revanced_disable_resuming_shorts_player'}
    settings=[dict(native=n,source=s,type='bool') for n,s in pairs.items()]
    for region in ('home','subscriptions','search','video_description','history'):settings.append(dict(native='hide_shorts',source='revanced_hide_shorts_'+region,type='bool'))
    settings.extend([dict(native='default_speed',source='revanced_playback_speed_default',type='f32'),dict(native='custom_speeds',source='revanced_custom_playback_speeds',type='speeds'),dict(native='sponsor_min_duration',source='sb_min_segment_duration',type='f32')])
    categories=[dict(native=n,source='sb_'+('highlight' if n=='poi_highlight' else n)) for n in ('sponsor','selfpromo','interaction','intro','outro','preview','hook','filler','music_offtopic','poi_highlight')]
    defaults={n:True for n in pairs};defaults.update(background_playback=True,picture_in_picture=True,hide_shorts=True,custom_speed_menu=True,diagnostics=False,default_speed=1.0,custom_speeds=[.25,.5,.75,1,1.25,1.5,1.75,2,2.5,3,4],sponsor_min_duration=0,sponsor_categories=['sponsor','selfpromo','interaction','intro','outro'],sponsor_behaviors={'sponsor':'skip-once','selfpromo':'manual-skip','interaction':'manual-skip','intro':'skip','outro':'skip'})
    titles={'video_ads':'Block video ads','feed_ads':'Hide feed ads','background_playback':'Background playback','picture_in_picture':'Picture in Picture','sponsorblock':'SponsorBlock','return_dislikes':'Return YouTube Dislike','custom_speed_menu':'Translated speed menu','remember_speed':'Remember playback speed','hide_shorts':'Hide Shorts feed items','disable_shorts_resume':'Disable returning to Shorts','hide_info_cards':'Hide info cards','hide_end_cards':'Hide end cards','hide_autoplay_preview':'Hide autoplay preview','disable_popup_panels':'Disable popup panels','diagnostics':'USB diagnostics'}
    # Only bound features are exposed. Extra generated classes remain available for later placements.
    defaults['disable_popup_panels']=False
    menu=[dict(key=k,title=v) for k,v in titles.items() if k!='disable_popup_panels']
    view_effects={name:key for key,names in {'hide_info_cards':['YTInfoCardButton','YTInfoCardTeaserContainerView','YTInfoCardDarkTeaserContainerView'],'hide_end_cards':['YTCreatorEndscreenView','YTEndscreenElementView','YTSimpleEndscreenElementView','YTPlaylistEndscreenElementView'],'hide_autoplay_preview':['YTAutonavPreviewView','YTAutonavBackgroundView','YTAutonavEndscreenView']}.items() for name in names}
    inventory={x['text'] for x in json.loads((PROJECT/'analysis/evidence/objc_classname.json').read_text())}
    if set(view_effects)-inventory:raise ValueError('Unknown native view classes')
    return dict(schema=1,version='0.4.7-translated',app_version=app['version'],app_uuid=app['uuid'],original_executable_sha256=app['executable_sha256'],original_archive_sha256=app['archive_sha256'],hooks=hooks,messages=messages,view_effects=view_effects,settings=settings,categories=categories,defaults=defaults,menu=menu,unbound_generated_features=['disable_popup_panels','disable_auto_captions'],legacy_ios_code_used=False,device_verified=False)

def stage(worktree,generation):
    prepare(worktree,generation)
    dest=worktree/'batch-delivery/integration';dest.mkdir(exist_ok=True)
    for p in ROOT.glob('*.py'):shutil.copy2(p,dest/p.name)
    shutil.copy2(ROOT/'AppHost.m',dest/'AppHost.m')
    data=catalog();(dest/'RVBIntegration.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
    strings=ET.parse(PROJECT/'revanced-patches-main/patches/src/main/resources/addresources/values/strings.xml')
    resources={s.attrib['name']:''.join(s.itertext()).replace("\\'","'").replace('\\n','\n') for s in strings.getroot().iter('string')}
    (dest/'RVBResources.json').write_text(json.dumps(resources,ensure_ascii=False)+'\n',encoding='utf-8',newline='\n')
    workflow=worktree/'.github/workflows/stateful-batch.yml';text=workflow.read_text()
    anchor='      - uses: actions/upload-artifact@'
    text=text.replace(anchor,'      - name: Compile fresh app integration\n        run: python batch-delivery/integration/build_host.py --source batch-delivery/integration/AppHost.m --core batch-output/native/TranslatedBatch.dylib --native batch-delivery/native --out batch-output/app\n      - name: Include generated app catalogs\n        run: cp batch-delivery/integration/RVB*.json batch-output/app/\n'+anchor)
    workflow.write_text(text,encoding='utf-8',newline='\n')
    (ROOT/'RVBIntegration.json').write_text(json.dumps(data,indent=2)+'\n')
    print(f'Staged fresh host: {len(data["hooks"])} ABI placements, {len(data["messages"])} descriptor schemas; no legacy source compiled')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--worktree',type=Path,required=True);p.add_argument('--generation',type=Path,required=True);a=p.parse_args();stage(a.worktree.resolve(),a.generation.resolve())
