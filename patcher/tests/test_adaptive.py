from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import plistlib
import zipfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import adaptive
from adaptive_abi import abi
from adaptive_image import Image
import recipe_catalog
from macho import PatchError
import patcher
import integration_check
from test_patcher import binary


class RecipeTests(unittest.TestCase):
    def extract(self, source):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);(p/'RVPort.m').write_text(source)
            return recipe_catalog.extract(p)

    def test_constant_loops_and_layered_hooks(self):
        source='''static void RVInstall(void) {
        for (NSString *cls in @[@"YTA",@"YTB"])
            RVHook(cls,@"play",@"v@:",NO,^id(IMP original,SEL sel) { return ^(id self) {}; });
        RVHook(@"YTA",@"play",@"v@:",NO,^id(IMP original,SEL sel) { return ^(id self) {}; });
        }'''
        c=self.extract(source)
        self.assertFalse(c['unresolved']);self.assertEqual(len(c['contracts']),2)
        self.assertEqual(len(c['contracts'][0]['sources']),2)

    def test_unknown_declaration_fails_catalog_generation(self):
        for source in ['''static void RVInstall(void) { RVHook(runtimeClass,@"play",@"v@:",NO,factory); }''',
                       '''static void RVInstall(void) { for (NSString *cls in runtimeClasses) RVHook(cls,@"play",@"v@:",NO,^id(IMP a,SEL b) { return nil; }); }''']:
            self.assertTrue(self.extract(source)['unresolved'])

    def test_live_catalog_exhaustive_and_fresh(self):
        c=adaptive.catalog()
        self.assertGreater(len(c['contracts']),350);self.assertEqual(c['unresolved'],[])
        scrubs=[r for r in c['contracts'] if r['selector'] in ('notifyDidStartScrubbingAtTime:','notifyDidEndScrubbingAtTime:')]
        self.assertEqual([r['abi'] for r in scrubs],['v@:@','v@:@'])


class ABITests(unittest.TestCase):
    def test_offsets_do_not_erase_struct_array_or_bitfield_details(self):
        for raw,expected in [('v24@0:8@16','v@:@'),('v40@0:8d16','v@:d'),
                             ('v64@0:8^[12i]16{Foo1=b32[3f]}24','v@:^[12i]{Foo1=b32[3f]}'),
                             ('{CGRect={CGPoint=dd}{CGSize=dd}}48@0:8','{CGRect={CGPoint=dd}{CGSize=dd}}@:')]:
            self.assertEqual(abi(raw),expected)
        self.assertNotEqual(abi('v24@0:8@16'),abi('v24@0:8d16'))
        for malformed in ('','z@:','v@:+','[i]','[3i','b','{X=i','^'):
            with self.subTest(malformed=malformed),self.assertRaises(ValueError): abi(malformed)

    def test_native_and_host_normalization_agree(self):
        root=recipe_catalog.ROOT
        zig=root.parent/'patcher/.tools/python/ziglang/zig.exe'
        compiler=[str(zig),'cc'] if zig.exists() else [shutil.which('cc')] if shutil.which('cc') else None
        if not compiler: self.skipTest('C compiler unavailable')
        vectors=['v24@0:8@16','v24@0:8d16','v64@0:8^[12i]16{Foo1=b32[3f]}24','@@:','v@:@@?','v@:+','[i]','b','{X=i','^','z@:']
        with tempfile.TemporaryDirectory() as folder:
            exe=Path(folder)/'abi.exe'
            result=subprocess.run(compiler+['-Wall','-Wextra','-Werror','-O2',str(root/'tests/abi.c'),'-o',str(exe)],capture_output=True,text=True,timeout=90)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(exe),*vectors],capture_output=True,text=True,timeout=10)
            expected=[]
            for v in vectors:
                try: expected.append(abi(v))
                except ValueError: expected.append('INVALID')
            self.assertEqual(result.stdout.splitlines(),expected)


class ResolverTests(unittest.TestCase):
    def test_ambiguous_and_changed_abi_never_authorized(self):
        r=dict(class_name='YTA',kind='-',selector='play',abi='v@:d')
        m=dict(kind='-',selector='play',encoding='v24@0:8@16',imp='0x1000')
        classes={'YTA':dict(methods=[m],superclass=None,external_superclass=None)}
        self.assertEqual(adaptive.resolve(classes,r,{})['status'],'abi_mismatch')
        classes['YTA']['methods'].append(m)
        self.assertEqual(adaptive.resolve(classes,r,{})['status'],'ambiguous_metadata')

    def test_dynamic_descriptor_field_requires_typed_accessor(self):
        f=dict(name='maximumPlaybackRate',number=2,flags=0,data_type=3)
        self.assertEqual(adaptive.runtime_field_abi(f,'maximumPlaybackRate'),'f@:')
        self.assertEqual(adaptive.runtime_field_abi(f,'setMaximumPlaybackRate:'),'v@:f')
        self.assertIsNone(adaptive.runtime_field_abi(f,'unrelated'))
        f['flags']=2
        self.assertEqual(adaptive.runtime_field_abi(f,'maximumPlaybackRate'),'@@:')

    def test_target_and_catalog_binding_reject_stale_profiles(self):
        cat=adaptive.catalog()
        info={'CFBundleIdentifier':'com.google.ios.youtube','CFBundleShortVersionString':'99.1.2','CFBundleExecutable':'YouTube'}
        image=type('Identity',(),{'uuid':'1'*32})()
        p=dict(schema=1,catalog_sha256=recipe_catalog.digest(cat),bundle=info['CFBundleIdentifier'],
               version='99.1.2',executable='YouTube',uuid=image.uuid,executable_sha256='2'*64,
               contracts=[dict(**r,resolution={'status':'missing'}) for r in cat['contracts']])
        adaptive.validate(p,info,image,'2'*64)
        for key,bad in [('uuid','3'*32),('version','99.1.3'),('catalog_sha256','4'*64),('executable_sha256','5'*64)]:
            changed=deepcopy(p);changed[key]=bad
            with self.subTest(key=key),self.assertRaises(PatchError): adaptive.validate(changed,info,image,'2'*64)
        changed=deepcopy(p);changed['contracts'][0]['abi']='v@:d'
        with self.assertRaises(PatchError): adaptive.validate(changed,info,image)

    def test_unsupported_and_truncated_images_rejected(self):
        for blob in (b'',b'\0'*64,struct.pack('<8I',0xfeedfacf,0x100000c,2,2,0,0,0,0)):
            with self.assertRaises((ValueError,struct.error)): Image(blob)


class AdaptiveArchiveTests(unittest.TestCase):
    """Synthetic archive receipts; the real IPA corpus exercises discovery."""
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.input=self.root/'original.ipa';self.output=self.root/'patched.ipa';self.library=self.root/'RVPort.dylib'
        self.main=binary()
        cat=adaptive.catalog()
        self.info=dict(CFBundleIdentifier='com.google.ios.youtube',CFBundleExecutable='YouTube',CFBundleShortVersionString='99.1.2')
        self.p=dict(schema=1,adapter_catalog=cat['adapter_catalog'],catalog_sha256=recipe_catalog.digest(cat),
                    id='fixture-adaptive',bundle=self.info['CFBundleIdentifier'],executable='YouTube',version='99.1.2',
                    uuid=bytes(range(16)).hex(),executable_sha256=hashlib.sha256(self.main).hexdigest(),
                    contracts=[dict(**r,resolution={'status':'missing'}) for r in cat['contracts']],
                    required_capabilities=cat['required_capabilities'],disabled_features=[],summary={})
        payload=bytearray(binary(6));tag=adaptive.payload_tag(self.p);payload[0x1300:0x1300+len(tag)]=tag
        self.library.write_bytes(payload)
        with zipfile.ZipFile(self.input,'w') as z:
            z.writestr('Payload/YouTube.app/Info.plist',plistlib.dumps(self.info))
            z.writestr('Payload/YouTube.app/YouTube',self.main)
            z.writestr('Payload/YouTube.app/resource.txt',b'unchanged')
        def fixture_discover(info,data):
            self.assertEqual(data,self.main,'Verifier must reconstruct the exact original')
            return deepcopy(self.p)
        self.mock=patch.object(adaptive,'discover_binary',side_effect=fixture_discover);self.mock.start()

    def tearDown(self):
        self.mock.stop();self.temp.cleanup()

    def test_default_path_packages_and_rediscover_verifies(self):
        marker=patcher.patch(self.input,self.output,self.library,{})
        self.assertEqual(marker['release_flavor'],'adaptive')
        result=integration_check.check(self.input,self.output)
        self.assertTrue(result['runtime_preflight_required'])
        self.assertTrue(result['section_and_linkedit_bytes_preserved'])
        with zipfile.ZipFile(self.output) as z:
            self.assertEqual(z.read('Payload/YouTube.app/RVPortProfile.json'),recipe_catalog.canonical(self.p))

    def test_old_payload_and_changed_reviewed_profile_refused(self):
        self.library.write_bytes(binary(6))
        with self.assertRaisesRegex(PatchError,'Payload does not contain'): patcher.patch(self.input,self.output,self.library,{})
        self.assertFalse(self.output.exists())
        payload=bytearray(binary(6));tag=adaptive.payload_tag(self.p);payload[0x1300:0x1300+len(tag)]=tag;self.library.write_bytes(payload)
        changed=deepcopy(self.p);changed['contracts'][0]['resolution']['status']='static_match'
        path=self.root/'review.json';path.write_bytes(recipe_catalog.canonical(changed))
        with self.assertRaisesRegex(PatchError,'differs from fresh discovery'): patcher.patch(self.input,self.output,self.library,{},adaptive_profile=path)
        self.assertFalse(self.output.exists())

    def test_tampered_profile_refused_even_with_recomputed_receipt(self):
        patcher.patch(self.input,self.output,self.library,{})
        for recompute in (False,True):
            corrupt=self.root/('corrupt-'+str(recompute)+'.ipa')
            changed=deepcopy(self.p);changed['contracts'][0]['resolution']['status']='static_match'
            data=recipe_catalog.canonical(changed)
            with zipfile.ZipFile(self.output) as src,zipfile.ZipFile(corrupt,'w') as dst:
                for entry in src.infolist():
                    content=src.read(entry)
                    if entry.filename.endswith('/RVPortProfile.json'): content=data
                    if recompute and entry.filename.endswith('/RVPortPatchManifest.json'):
                        marker=json.loads(content);marker['adaptive_profile_sha256']=patcher.sha(data);content=json.dumps(marker).encode()
                    dst.writestr(entry,content)
            with self.subTest(recompute=recompute),self.assertRaises(PatchError): patcher.verify(corrupt)

    def test_adaptive_binding_cannot_be_removed_by_downgrading_manifest(self):
        with self.assertRaises(PatchError): patcher.patch(self.input,self.output,self.library,{},adaptive_profile=False)
        patcher.patch(self.input,self.output,self.library,{})
        corrupt=self.root/'downgraded.ipa'
        with zipfile.ZipFile(self.output) as src,zipfile.ZipFile(corrupt,'w') as dst:
            for entry in src.infolist():
                data=src.read(entry)
                if entry.filename.endswith('/RVPortPatchManifest.json'):
                    marker=json.loads(data);marker.pop('adaptive_profile_sha256');marker['release_flavor']='legacy';data=json.dumps(marker).encode()
                dst.writestr(entry,data)
        with self.assertRaisesRegex(PatchError,'cannot be downgraded'): patcher.verify(corrupt)


if __name__=='__main__': unittest.main()
