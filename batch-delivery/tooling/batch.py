"""Generate the requested larger batch from attributed Android source closures."""
import argparse
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import regions
import roundtrip
import facades
import lowering

ROOT=Path(__file__).resolve().parent
RE=ROOT.parent.parent
UPSTREAM=RE/'revanced-patches-main'
YT=UPSTREAM/'extensions/youtube/src/main/java'
SHARED=UPSTREAM/'extensions/shared/library/src/main/java'
GSON=ROOT.parent/'.tools/gson-2.13.2.jar'
ANDROID=ROOT.parent/'.tools/android-all-14.jar'
JSON_JAR=ROOT.parent/'.tools/j2objc/dist/lib/json.jar'
YP='app/revanced/extension/youtube/'
SP='app/revanced/extension/shared/'
FILES={
 'ads':['patches/VideoAdsPatch.java','patches/litho/AdsFilter.java'],
 'background_pip':['patches/BackgroundPlaybackPatch.java'],
 'sponsorblock':['sponsorblock/SegmentPlaybackController.java','sponsorblock/objects/SponsorSegment.java','sponsorblock/objects/CategoryBehaviour.java',
                 'sponsorblock/objects/SegmentCategory.java','sponsorblock/objects/UserStats.java','sponsorblock/requests/SBRequester.java','sponsorblock/requests/SBRoutes.java',
                 'sponsorblock/SponsorBlockSettings.java','sponsorblock/SponsorBlockUtils.java'],
 'speed':['patches/playback/speed/RememberPlaybackSpeedPatch.java','patches/playback/speed/CustomPlaybackSpeedPatch.java'],
 'shorts':['patches/litho/ShortsFilter.java','patches/DisableResumingStartupShortsPlayerPatch.java'],
 'player_cleanup':['patches/HidePlayerOverlayButtonsPatch.java','patches/HideInfoCardsPatch.java','patches/HideEndScreenCardsPatch.java',
                   'patches/HideRelatedVideoOverlayPatch.java','patches/HideAutoplayPreviewPatch.java','patches/DisablePlayerPopupPanelsPatch.java'],
 'ryd':['returnyoutubedislike/ReturnYouTubeDislike.java','returnyoutubedislike/requests/RYDVoteData.java','returnyoutubedislike/requests/ReturnYouTubeDislikeAPI.java',
        'returnyoutubedislike/requests/ReturnYouTubeDislikeRoutes.java','patches/ReturnYouTubeDislikePatch.java','patches/litho/ReturnYouTubeDislikeFilter.java'],
}
SHARED_FILES=['TrieSearch.java','StringTrieSearch.java','ByteTrieSearch.java','patches/litho/Filter.java','patches/litho/FilterGroup.java','patches/litho/FilterGroupList.java',
              'requests/Route.java','requests/Requester.java']


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,text):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text,encoding='utf-8',newline='\n')
def save(p,value):write(p,json.dumps(value,indent=2)+'\n')
def run(command,log):
    with log.open('w',encoding='utf-8') as f:r=subprocess.run(list(map(str,command)),stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(log.read_text(errors='replace')[-15000:])
def compile_sources(src,out,log,classpath=''):
    out.mkdir();run(['javac','-encoding','UTF-8','-cp',classpath,'-d',out,*sorted(src.rglob('*.java'))],log)


def generate(out):
    from kit import generate_kit
    if out.exists():raise ValueError('Fresh batch output required')
    out.mkdir(parents=True)
    compiler=out/'exporter';compiler.mkdir()
    run(['javac','-cp',GSON,'-d',compiler,*[ROOT/n for n in ('ExportBatch.java','ExportSettings.java','SliceSource.java','ExtractPatterns.java')]],out/'compiler-javac.log')
    originals={YT/(YP+f) for fs in FILES.values() for f in fs}|{SHARED/(SP+f) for f in SHARED_FILES}
    originals.add(YT/(YP+'patches/VersionCheckPatch.java'))
    raw=out/'retained';original=out/'original'
    for path in sorted(originals):
        base=YT if path.is_relative_to(YT) else SHARED;rel=path.relative_to(base)
        if not path.exists():raise ValueError('Missing batch source '+str(path))
        (raw/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,raw/rel)
        write(original/rel,path.read_text(encoding='utf-8'))
    run(['java','-cp',str(GSON)+os.pathsep+str(compiler),'ExportSettings',YT/(YP+'settings/Settings.java'),out/'all-setting-facts.json'],out/'settings-export.log')
    kit=out/'kit';generate_kit(kit,original,json.loads((out/'all-setting-facts.json').read_text(encoding='utf-8')),out)
    allsrc=out/'analysis-src';shutil.copytree(original,allsrc);shutil.copytree(kit,allsrc,dirs_exist_ok=True)
    compile_sources(allsrc,out/'analysis-classes',out/'original-javac.log',str(ANDROID))
    args=['java','-cp',str(GSON)+os.pathsep+str(compiler),'ExportBatch',out/'attributed.json',str(out/'analysis-classes')+os.pathsep+str(ANDROID),'',*sorted(original.rglob('*.java'))]
    run(args,out/'attribution.log')
    module=json.loads((out/'attributed.json').read_text(encoding='utf-8'))
    # Reviewed Java runtime and operation facade descriptors are generated from this actual attributed slice.
    contracts={};external_fields={};frontier=[]
    for n in regions.walk(module):
        target=n.get('target')
        if not isinstance(target,str):target=None
        if target and target.startswith(('java.','javax.crypto.','org.json.','android.icu.')):
            if any(x in target for x in ['#exit(','java.lang.reflect.','java.lang.Runtime#exec']):frontier.append(target)
            else:contracts[target]=dict(family='pinned-java-runtime',effects=['java.runtime'])
        elif target and any(target.startswith(prefix) for prefix in ['android.','androidx.','kotlin.']):contracts[target]=dict(family='reviewed-platform-surface',effects=['platform.operation'])
        elif target and any(target.startswith(prefix) for prefix in ['app.revanced.extension.shared.Utils#','app.revanced.extension.shared.Logger#','app.revanced.extension.shared.StringRef#',
                        'app.revanced.extension.shared.settings.','app.revanced.extension.youtube.shared.','app.revanced.extension.youtube.patches.VideoInformation#',
                        'app.revanced.extension.youtube.videoplayer.PlayerControlButton#','app.revanced.extension.shared.ui.','app.revanced.extension.youtube.sponsorblock.ui.SponsorBlockViewController#',
                        'app.revanced.extension.youtube.patches.litho.PlaybackSpeedMenuFilter#']):contracts[target]=dict(family='reviewed-operation-adapter',effects=['platform.operation'])
        if n.get('field') and n.get('symbol'):
            external_fields[n['symbol']]=dict(type=n.get('type'),origin=n.get('origin'))
    if frontier:raise ValueError('Unsafe runtime operations '+str(frontier))
    module=regions.annotate(module,contracts);save(out/'module.json',module);save(out/'contracts.json',contracts);save(out/'external-fields.json',external_fields)
    admission=regions.verify(module,contracts,external_fields)
    generated=out/'generated'
    for name,text in regions.emit_java(module,contracts,external_fields).items():write(generated/name,text)
    full=out/'generated-src';shutil.copytree(generated,full);shutil.copytree(kit,full,dirs_exist_ok=True)
    compile_sources(full,out/'generated-classes',out/'generated-javac.log',str(ANDROID))
    run(['java','-cp',str(GSON)+os.pathsep+str(compiler),'ExportBatch',out/'reattributed.json',str(out/'generated-classes')+os.pathsep+str(ANDROID),'',*sorted(generated.rglob('*.java'))],out/'reattribution.log')
    reattribution=roundtrip.check(module,json.loads((out/'reattributed.json').read_text(encoding='utf-8')))
    native_module=lowering.binding_patterns(lowering.platform_types(module))
    native_contracts=lowering.platform_types(contracts)
    native_contracts['java.lang.System#currentTimeMillis()']=dict(family='wall-clock',effects=['clock.wall'],emit_call='rv.batch.Platform.wallMillis')
    native_contracts['java.net.URL#openConnection()']=dict(family='http-transport',effects=['http.allocate'],emit_call='rv.batch.Network.open',receiver_first=True)
    native_fields=lowering.platform_types(external_fields)
    native_src=out/'portable-src'
    for name,text in regions.emit_java(native_module,native_contracts,native_fields).items():write(native_src/name,text)
    run(['java','-cp',str(GSON)+os.pathsep+str(compiler)+os.pathsep+str(ANDROID),'ExtractPatterns',out/'web-pattern.json'],out/'pattern-export.log')
    shutil.copytree(kit,native_src,dirs_exist_ok=True);facades.generate(native_src,json.loads((out/'web-pattern.json').read_text(encoding='utf-8')))
    for name in ('Network.java','NativeOperations.java','Events.java'):write(native_src/('rv/batch/'+name),(ROOT/name).read_text(encoding='utf-8'))
    compile_sources(native_src,out/'portable-classes',out/'portable-javac.log',str(JSON_JAR))
    save(out/'portable-module.json',native_module);save(out/'portable-contracts.json',native_contracts)
    save(out/'generation-receipt.json',dict(admission=admission,graph_sha256=regions.graph_hash(module),policy_generated_from_ir=True,
            reattribution=reattribution,
            source_sha256={str(p):sha(p) for p in sorted(originals)},batch=FILES,whole_app_hooks_installed=False,device_verified=False))
    print(json.dumps(admission,indent=2))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);a=p.parse_args();generate(a.out.resolve())
if __name__=='__main__':main()
