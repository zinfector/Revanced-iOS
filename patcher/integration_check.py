"""Compare a real original and unsigned patched IPA, including all retained member contents."""
import argparse
import hashlib
import json
import plistlib
from pathlib import Path
from pathlib import PurePosixPath
import zipfile

from macho import LC_CODE_SIGNATURE, MachO
import patcher


def digest_member(archive, name):
    with archive.open(name) as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check(source, target):
    result = patcher.verify(target)
    with zipfile.ZipFile(source) as before, zipfile.ZipFile(target) as after:
        app, info, original, original_image = patcher.read_app(before)
        patched_app, _, modified, modified_image = patcher.read_app(after)
        assert app == patched_app
        assert after.testzip() is None, 'ZIP CRC failure'
        assert original[original_image.first_content:] == modified[modified_image.first_content:], 'Section or LINKEDIT bytes changed'
        preserved_commands = [c.raw for c in original_image.commands if c.kind != LC_CODE_SIGNATURE]
        assert [c.raw for c in modified_image.commands[:-1]] == preserved_commands, 'Existing load command changed'
        manifest = json.loads(after.read(app+'/'+patcher.STAMP_NAME))
        assert patcher.file_sha(source) == manifest['input_ipa_sha256'], 'Input archive hash mismatch'
        assert patcher.sha(original) == manifest['original_executable_sha256'], 'Original executable hash mismatch'
        expected_new = {app+'/Frameworks/'+patcher.PAYLOAD_NAME, app+'/'+patcher.CONFIG_NAME, app+'/'+patcher.STAMP_NAME}
        if manifest.get('adaptive_profile_sha256'): expected_new.add(app+'/RVPortProfile.json')
        expected_new.update(app+'/'+name for name in manifest.get('resources',{}) if app+'/'+name not in before.namelist())
        assert set(after.namelist()) - set(before.namelist()) == expected_new, 'Unexpected new files'
        skipped = set()
        preserved = 0
        main = app+'/'+info['CFBundleExecutable']
        for name in before.namelist():
            parts = PurePosixPath(name).parts
            signature = name.startswith(app+'/') and ('_CodeSignature' in parts or parts[-1]=='embedded.mobileprovision')
            extension = manifest['extensions_removed'] and name.startswith(app+'/') and any(p.endswith('.appex') for p in parts)
            if signature or extension:
                skipped.add(name)
                assert name not in after.namelist(), 'Old signature or explicitly removed extension retained'
            elif name != main and name != app+'/Info.plist':
                assert digest_member(before, name) == digest_member(after, name), 'Retained member changed: '+name
                preserved += 1
            elif name == app+'/Info.plist':
                if 'Info.plist' in manifest.get('resources',{}):
                    assert digest_member(after,name)==manifest['resources']['Info.plist'], 'Branding Info.plist hash mismatch'
                    changed_info=plistlib.loads(after.read(name))
                    allowed={'CFBundleDisplayName','CFBundleName','CFBundleIcons','CFBundleIcons~ipad'}
                    assert {k:v for k,v in changed_info.items() if k not in allowed}=={k:v for k,v in info.items() if k not in allowed}, 'Unrelated Info.plist field changed'
                else:
                    assert digest_member(before,name)==digest_member(after,name), 'Info.plist changed without branding manifest'
                    preserved+=1
        assert set(before.namelist()) - set(after.namelist()) == skipped, 'Unexpected missing files'
        result.update(input_ipa_sha256=patcher.file_sha(source), output_ipa_sha256=patcher.file_sha(target),
                      retained_members_identical=preserved, removed_members=len(skipped),
                      existing_load_commands_preserved=True, section_and_linkedit_bytes_preserved=True,
                      zip_crc_verified=True, output_size=Path(target).stat().st_size)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('-o', '--output', type=Path)
    args = parser.parse_args()
    result = json.dumps(check(args.source, args.target), indent=2)
    if args.output: args.output.write_text(result, encoding='utf-8')
    print(result)


if __name__ == '__main__': main()
