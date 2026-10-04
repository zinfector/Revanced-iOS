"""Prepare a checked unsigned IPA for the manual GitHub signing workflow."""
import json
import os
from pathlib import Path
import tempfile

from cloud_download import DownloadError, download
from patcher import ROOT, patch, profile, verify


def main():
    mode = os.environ.get('IPA_KIND', 'original')
    preset = os.environ.get('PATCH_PRESET', 'expanded')
    strip = os.environ.get('STRIP_EXTENSIONS', 'true')
    if mode not in ('original', 'patched') or preset not in ('defaults', 'expanded', 'sideload-auth') or strip not in ('true', 'false'):
        raise DownloadError('Unsupported workflow input')
    output = ROOT / 'build/cloud'
    output.mkdir(parents=True, exist_ok=True)
    target = output / 'unsigned.ipa'
    expected = os.environ.get('IPA_SHA256', '').strip()
    if mode == 'original':
        expected = expected or '37fd59f89d706fb7f93614e12ddb9c09fe3f4a18c1e4609c2b715db6e9f3d88c'
    with tempfile.TemporaryDirectory(prefix='rvport-download-', dir=os.environ.get('RUNNER_TEMP')) as temp:
        source = Path(temp) / 'source.ipa'
        digest = download(os.environ.get('IPA_URL', ''), source, expected)
        if mode == 'original':
            config = json.loads((ROOT / f'configs/{preset}.json').read_text(encoding='utf-8'))
            patch(source, target, ROOT / 'build/RVPort.dylib', config, strip_extensions=strip == 'true')
        else:
            # Verify the injected library, manifest, UUID and config before accepting it for signing.
            verify(source)
            if target.exists():
                raise DownloadError('Unsigned output already exists')
            source.replace(target)
        report = verify(target)
        report.update(source_sha256=digest, source_kind=mode, device_validated=False)
        (output / 'unsigned-verification.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print('Unsigned IPA prepared and verified for profile ' + profile()['id'])


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, ValueError, OSError) as error:
        raise SystemExit(str(error)) from None
