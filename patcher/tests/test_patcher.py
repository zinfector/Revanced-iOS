import hashlib
import json
from pathlib import Path
import plistlib
import struct
import subprocess
import sys
import tempfile
import zlib
import unittest
from unittest.mock import patch as mock
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import macho
import patcher
import branding
import integration_check

def dylib_command(kind,path):
    name=path.encode()+b'\0';n=(24+len(name)+7)&~7
    return struct.pack('<6I',kind,n,24,0,0x10000,0x10000)+name+bytes(n-24-len(name))

def binary(filetype=2,padding=True,encrypted=False):
    offset=0x1000
    section=struct.pack('<16s16sQQ8I',b'__text',b'__TEXT',0x100001000,16,offset,2,0,0,0x80000400,0,0,0)
    segment=struct.pack('<II16s4Q4I',0x19,72+80,b'__TEXT',0x100000000,0x3000,0,0x3000,7,5,1,0)+section
    cmds=[segment,struct.pack('<II16s',0x1b,24,bytes(range(16))),
          struct.pack('<6I',0x32,24,2,17<<16,17<<16,0)]
    if filetype==2:
        cmds += [struct.pack('<6I',0x2c,24,0x1000,16,int(encrypted),0),
                 struct.pack('<4I',0x80000034,16,0x1100,16),
                 struct.pack('<4I',0x1d,16,0x2000,16)]
    else:cmds.append(dylib_command(0xd,patcher.LOAD_PATH))
    table=b''.join(cmds);header=struct.pack('<8I',0xfeedfacf,0x100000c,0,filetype,len(cmds),len(table),0x200085,0)
    blob=bytearray(header+table+bytes(0x3000-len(header)-len(table)))
    blob[0x1000:0x1010]=bytes(range(16));blob[0x1100:0x1110]=b'fixups unchanged'
    if not padding:blob[len(header)+len(table):0x1000]=b'X'*(0x1000-len(header)-len(table))
    return bytes(blob)

def png_bytes(width,height,filter_byte=0):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
    pixels=(bytes([filter_byte])+bytes(width*4))*height
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b'')

class MachOTests(unittest.TestCase):
    def test_injection_preserves_section_bytes_and_chained_fixups(self):
        original=binary();result=macho.inject_library(original,patcher.LOAD_PATH)
        image=macho.MachO(result)
        self.assertEqual(result[0x1000:],original[0x1000:])
        self.assertEqual(len(result),len(original))
        self.assertIn((macho.LC_LOAD_DYLIB,patcher.LOAD_PATH),image.dylibs())
        self.assertFalse(any(c.kind==macho.LC_CODE_SIGNATURE for c in image.commands))
        self.assertEqual([c.raw for c in image.commands if c.kind==macho.LC_DYLD_CHAINED_FIXUPS],
                         [c.raw for c in macho.MachO(original).commands if c.kind==macho.LC_DYLD_CHAINED_FIXUPS])

    def test_refuses_nonzero_padding_and_duplicate_injection(self):
        with self.assertRaises(macho.PatchError):macho.inject_library(binary(padding=False),patcher.LOAD_PATH)
        result=macho.inject_library(binary(),patcher.LOAD_PATH)
        with self.assertRaises(macho.PatchError):macho.inject_library(result,patcher.LOAD_PATH)

    def test_rejects_encrypted_fat_arm64e_and_malformed_commands(self):
        with self.assertRaises(macho.PatchError):macho.inject_library(binary(encrypted=True),patcher.LOAD_PATH)
        for offset,value in [(0,0xcafebabe),(8,2),(36,7),(20,0xfffffff8)]:
            blob=bytearray(binary());struct.pack_into('<I',blob,offset,value)
            with self.assertRaises(macho.PatchError):macho.MachO(bytes(blob))

class NativeIntervalTests(unittest.TestCase):
    def test_actual_native_interval_and_marker_helper(self):
        root=Path(__file__).resolve().parents[1]
        zig=root/'.tools/python/ziglang/zig.exe'
        import shutil
        compiler=[str(zig),'cc'] if zig.exists() else [shutil.which('cc')] if shutil.which('cc') else None
        if not compiler:self.skipTest('A C compiler is required for the shared native helper test')
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'intervals.exe'
            compiled=subprocess.run(compiler+['-Wall','-Wextra','-Werror','-O2',str(root/'tests/intervals.c'),'-o',str(output)],capture_output=True,text=True,timeout=60)
            self.assertEqual(compiled.returncode,0,compiled.stdout+compiled.stderr)
            result=subprocess.run([str(output)],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('checks passed',result.stdout)

class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.input=self.root/'original.ipa';self.output=self.root/'patched.ipa';self.library=self.root/'RVPort.dylib'
        self.main=binary();self.library.write_bytes(binary(6))
        self.p={'id':'fixture','bundle':'com.google.ios.youtube','version':'21.39.4',
                'uuid':bytes(range(16)).hex(),'executable_sha256':hashlib.sha256(self.main).hexdigest()}
        self.profile_mock=mock.object(patcher,'profile',return_value=self.p);self.profile_mock.start()
        self.make_archive()
    def tearDown(self):self.profile_mock.stop();self.temp.cleanup()
    def make_archive(self,extra=None):
        with zipfile.ZipFile(self.input,'w') as z:
            z.writestr('Payload/YouTube.app/Info.plist',plistlib.dumps({'CFBundleIdentifier':self.p['bundle'],
                'CFBundleShortVersionString':self.p['version'],'CFBundleExecutable':'YouTube'}))
            z.writestr('Payload/YouTube.app/YouTube',self.main)
            z.writestr('Payload/YouTube.app/ordinary-resource.txt','original content')
            z.writestr('Payload/YouTube.app/_CodeSignature/CodeResources','old signature')
            z.writestr('Payload/YouTube.app/embedded.mobileprovision','old profile')
            z.writestr('Payload/YouTube.app/PlugIns/Share.appex/extension','keep me')
            if extra:
                for name,data in extra:z.writestr(name,data)

    def test_round_trip_manifest_original_preservation_and_signature_cleanup(self):
        before=self.input.read_bytes()
        marker=patcher.patch(self.input,self.output,self.library,{'sponsorblock':True,'default_speed':1.25})
        self.assertEqual(before,self.input.read_bytes());self.assertTrue(marker['signing_required'])
        self.assertEqual(patcher.verify(self.output)['status'],'verified')
        with zipfile.ZipFile(self.output) as z:
            self.assertEqual(z.read('Payload/YouTube.app/ordinary-resource.txt'),b'original content')
            self.assertNotIn('Payload/YouTube.app/_CodeSignature/CodeResources',z.namelist())
            self.assertNotIn('Payload/YouTube.app/embedded.mobileprovision',z.namelist())
            self.assertIn('Payload/YouTube.app/PlugIns/Share.appex/extension',z.namelist())
            self.assertEqual(json.loads(z.read('Payload/YouTube.app/RVPort.json'))['default_speed'],1.25)

    def test_refuses_existing_output_and_repatch_and_input_overwrite(self):
        with self.assertRaises(macho.PatchError):patcher.patch(self.input,self.input,self.library,{})
        patcher.patch(self.input,self.output,self.library,{})
        before=self.output.read_bytes()
        with self.assertRaises(macho.PatchError):patcher.patch(self.input,self.output,self.library,{})
        self.assertEqual(before,self.output.read_bytes())
        with self.assertRaises(macho.PatchError):patcher.patch(self.output,self.root/'again.ipa',self.library,{})

    def test_profile_hash_mismatch_never_creates_output(self):
        self.p['executable_sha256']='0'*64
        with self.assertRaises(macho.PatchError):patcher.patch(self.input,self.output,self.library,{})
        self.assertFalse(self.output.exists())

    def test_archive_traversal_duplicates_and_symlink_rejected(self):
        for name in ('../escape','Payload/../escape','C:/escape'):
            self.make_archive([(name,b'x')])
            with self.assertRaises(macho.PatchError):patcher.inspect(self.input)
        # ZipInfo canonicalizes backslashes on Windows when authoring an archive;
        # validate the raw untrusted name without that authoring normalization.
        class UnsafeArchive:
            def infolist(self):
                info=zipfile.ZipInfo('ordinary');info.filename='Payload\\escape';return [info]
        with self.assertRaises(macho.PatchError):patcher.members(UnsafeArchive())
        with zipfile.ZipFile(self.input,'a') as z:
            symlink=zipfile.ZipInfo('Payload/link');symlink.external_attr=0o120777<<16;z.writestr(symlink,'../escape')
        with self.assertRaises(macho.PatchError):patcher.inspect(self.input)
        self.make_archive([('Payload/YouTube.app/YouTube',self.main)])
        with self.assertRaises(macho.PatchError):patcher.inspect(self.input)

    def test_extensions_removed_only_when_requested(self):
        patcher.patch(self.input,self.output,self.library,{},True)
        with zipfile.ZipFile(self.output) as z:self.assertFalse(any('.appex/' in n for n in z.namelist()))

    def test_tamper_detection(self):
        patcher.patch(self.input,self.output,self.library,{})
        corrupt=self.root/'corrupt.ipa'
        with zipfile.ZipFile(self.output) as src,zipfile.ZipFile(corrupt,'w') as dst:
            for info in src.infolist():
                data=src.read(info)
                if info.filename.endswith('/RVPort.dylib'):data=data[:-1]+bytes([data[-1]^1])
                dst.writestr(info,data)
        with self.assertRaises(macho.PatchError):patcher.verify(corrupt)

    def test_config_and_payload_validation(self):
        for c in ({'sponsorblock':1},{'default_speed':float('nan')},{'default_speed':4.1},
                  {'default_quality':999},{'unknown':True},{'feed_patterns':['']},
                  {'sponsor_categories':['unknown']},{'ad_strategy':'bad'},{'schema':True}):
            with self.assertRaises(macho.PatchError):patcher.validate_config(c)
        self.library.write_bytes(binary(2))
        with self.assertRaises(macho.PatchError):patcher.patch(self.input,self.output,self.library,{})

    def test_expanded_features_round_trip(self):
        config={key:True for key in patcher.FEATURES}
        config.update(double_tap_seconds=15,theme='dark',seekbar_color='#123ABC',thumbnail_proxy_url='https://example.test/image',custom_speeds=[.25,1,4])
        marker=patcher.patch(self.input,self.output,self.library,config)
        result=patcher.verify(self.output)
        self.assertTrue(all(result['features'].values()))
        with zipfile.ZipFile(self.output) as z:
            stored=json.loads(z.read('Payload/YouTube.app/RVPort.json'))
            self.assertEqual(stored,marker['config'])
            self.assertEqual(stored['double_tap_seconds'],15)
            self.assertEqual(stored['theme'],'dark')

    def test_extended_config_rejects_invalid_numbers_urls_and_types(self):
        for config in ({'custom_speeds':[]},{'custom_speeds':[True]},{'custom_speeds':[float('inf')]},
                       {'double_tap_seconds':121},{'double_tap_seconds':float('nan')},
                       {'overlay_opacity':-1},{'overlay_opacity':True},{'seekbar_color':'#123'},
                       {'theme':'unknown'},{'start_page':'bad'},{'thumbnail_frame':True},
                       {'client_version':'bad'},{'screen_width_points':0},{'screen_height_points':True},
                       {'form_factor':'automotive'},{'app_name':'bad\nname'},
                       {'thumbnail_proxy_url':'http://example.test/'},{'thumbnail_proxy_url':'https://user:pass@example.test/'},
                       {'thumbnail_proxy_url':'https://example.test/#fragment'},{'thumbnail_proxy_url':1}):
            with self.subTest(config=config),self.assertRaises(macho.PatchError):patcher.validate_config(config)

    def test_sponsor_category_policies_and_dearrow_configuration(self):
        config={'sponsor_categories':['sponsor','hook','poi_highlight'],
                'sponsor_behaviors':{'sponsor':'skip-once','hook':'manual-skip','poi_highlight':'seekbar-only'},
                'sponsor_colors':{'sponsor':'#010203'},'sponsor_min_duration':.5,
                'dearrow_thumbnails':True,'sponsorblock_markers':True}
        marker=patcher.patch(self.input,self.output,self.library,config)
        self.assertEqual(marker['config']['sponsor_behaviors'],config['sponsor_behaviors'])
        self.assertTrue(patcher.verify(self.output)['features']['dearrow_thumbnails'])
        for invalid in ({'sponsor_behaviors':{'sponsor':'unknown'}},{'sponsor_behaviors':{'bad':'skip'}},
                        {'sponsor_behaviors':[]},{'sponsor_colors':{'intro':'#12'}},{'sponsor_colors':{'bad':'#123456'}},
                        {'sponsor_min_duration':True},{'sponsor_min_duration':float('inf')},{'sponsor_min_duration':601},
                        {'dearrow_url':'http://example.test/'},{'dearrow_url':'https://user@example.test/'},
                        {'dearrow_url':'https://example.test/#fragment'},{'dearrow_url':1}):
            with self.subTest(config=invalid),self.assertRaises(macho.PatchError):patcher.validate_config(invalid)

    def test_thumbnail_contexts_and_theme_palette_configuration(self):
        config={'thumbnail_modes':{'home':'dearrow-stills','subscriptions':'original','player':'stills','search':'dearrow'},
                'theme_light_background':'#FFFFFF','theme_dark_background':'#101010','fast_thumbnail_stills':True}
        marker=patcher.patch(self.input,self.output,self.library,config)
        self.assertEqual(marker['config']['thumbnail_modes'],config['thumbnail_modes'])
        self.assertEqual(patcher.verify(self.output)['status'],'verified')
        for invalid in ({'thumbnail_modes':{'other':'stills'}},{'thumbnail_modes':{'home':'invalid'}},
                        {'thumbnail_modes':[]},{'theme_light_background':'red'},{'theme_dark_background':'#123'},{'theme_dark_background':1}):
            with self.subTest(config=invalid),self.assertRaises(macho.PatchError):patcher.validate_config(invalid)

    def test_network_quality_policies(self):
        marker=patcher.patch(self.input,self.output,self.library,{'wifi_quality':1080,'cellular_quality':480,'remember_quality':True})
        self.assertEqual(marker['config']['wifi_quality'],1080)
        self.assertEqual(marker['config']['cellular_quality'],480)
        self.assertEqual(patcher.verify(self.output)['status'],'verified')
        for config in ({'wifi_quality':True},{'cellular_quality':999},{'wifi_quality':-2}):
            with self.subTest(config=config),self.assertRaises(macho.PatchError):patcher.validate_config(config)

    def test_branding_round_trip_and_integrity(self):
        assets={}
        for key,size in [('header_image',32),('icon_120',120),('icon_180',180),('icon_152',152)]:
            path=self.root/(key+'.png');path.write_bytes(png_bytes(size,size));assets[key]=path
        before=self.input.read_bytes()
        marker=patcher.patch(self.input,self.output,self.library,{'app_name':'My YouTube'},branding=assets)
        self.assertEqual(self.input.read_bytes(),before)
        self.assertTrue(marker['config']['custom_header'])
        self.assertEqual(len(marker['resources']),5)
        integration_check.check(self.input,self.output)
        with zipfile.ZipFile(self.output) as z:
            info=plistlib.loads(z.read('Payload/YouTube.app/Info.plist'))
            self.assertEqual(info['CFBundleDisplayName'],'My YouTube')
            self.assertEqual(info['CFBundleIcons']['CFBundlePrimaryIcon']['CFBundleIconFiles'],['RVAppIcon'])
        corrupt=self.root/'badbranding.ipa'
        with zipfile.ZipFile(self.output) as src,zipfile.ZipFile(corrupt,'w') as dst:
            for info in src.infolist():
                data=src.read(info)
                if info.filename.endswith('/RVHeader.png'):data=b'changed'
                dst.writestr(info,data)
        with self.assertRaises(macho.PatchError):patcher.verify(corrupt)

    def test_png_validation_and_incomplete_icon_pair(self):
        asset=self.root/'image.png';asset.write_bytes(png_bytes(120,120))
        self.assertEqual(branding.png(asset,(120,120)),asset.read_bytes())
        with self.assertRaises(macho.PatchError):branding.png(asset,(180,180))
        with self.assertRaises(macho.PatchError):patcher.patch(self.input,self.output,self.library,{},branding={'icon_120':asset})
        self.assertFalse(self.output.exists())
        for data in (b'not PNG',png_bytes(8,8,5),png_bytes(8,8)[:-1],png_bytes(8,8)+b'trailing'):
            asset.write_bytes(data)
            with self.assertRaises(macho.PatchError):branding.png(asset)

    def test_legacy_configuration_can_be_verified(self):
        patcher.patch(self.input,self.output,self.library,{})
        legacy=self.root/'legacy.ipa'
        with zipfile.ZipFile(self.output) as src,zipfile.ZipFile(legacy,'w') as dst:
            for info in src.infolist():
                data=src.read(info)
                if info.filename.endswith('/RVPort.json'):data=json.dumps({'schema':1}).encode()
                if info.filename.endswith('/RVPortPatchManifest.json'):
                    marker=json.loads(data);marker['patcher_version']='0.1.0';marker['config']={'schema':1};data=json.dumps(marker).encode()
                dst.writestr(info,data)
        self.assertEqual(patcher.verify(legacy)['status'],'verified')

if __name__=='__main__':unittest.main()
