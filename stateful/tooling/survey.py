"""Inventory actual upstream syntax without changing sources or old receipts."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
REVANCED = ROOT.parent
YOUTUBE = REVANCED / 'revanced-patches-main/extensions/youtube/src/main/java/app/revanced/extension/youtube'
SHARED = REVANCED / 'revanced-patches-main/extensions/shared/library/src/main/java/app/revanced/extension/shared'
JAVA_INPUTS = [YOUTUBE / p for p in (
    'patches/playback/speed/RememberPlaybackSpeedPatch.java',
    'patches/VideoInformation.java', 'patches/ReturnYouTubeDislikePatch.java',
    'returnyoutubedislike/ReturnYouTubeDislike.java',
    'returnyoutubedislike/requests/RYDVoteData.java',
    'returnyoutubedislike/requests/ReturnYouTubeDislikeAPI.java',
    'sponsorblock/SegmentPlaybackController.java', 'sponsorblock/objects/SponsorSegment.java')]
JAVA_INPUTS.append(SHARED / 'Utils.java')
IMPLEMENTATION_INPUTS = [REVANCED / p for p in (
    'editable-code/core.py', 'editable-code/backends.py', 'editable-code/capabilities.py',
    'source-reuse/compiler.py', 'source-reuse/JavaTreeDump.java', 'source-reuse/j2objc_backend.py',
    'capability-mapper/contracts.json', 'capability-mapper/generation.py',
    'runtime-integration/cloud-build-0.4.6/patcher/native/RVSpeed.inc')]
KOTLIN_INPUTS = [REVANCED / 'revanced-patches-main/patches/src/main/kotlin/app/revanced/patches/youtube' / p for p in (
    'video/speed/remember/RememberPlaybackSpeedPatch.kt',
    'layout/returnyoutubedislike/ReturnYouTubeDislikePatch.kt',
    'layout/sponsorblock/SponsorBlockPatch.kt')]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'investigation' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists():
        raise ValueError('Fresh investigation output required')
    jar = REVANCED.parent / 'jadx-gui-1.5.6-with-jre-win/lib/jadx-gui-1.5.6-all.jar'
    java, javac = (shutil.which(tool) for tool in ('java', 'javac'))
    if not java or not javac:
        raise RuntimeError('JDK java and javac required')
    inputs = [*JAVA_INPUTS, *IMPLEMENTATION_INPUTS, *KOTLIN_INPUTS, ROOT / 'PatchSurvey.java', Path(__file__), jar, Path(java), Path(javac)]
    before = {str(p): digest(p) for p in inputs}
    out.mkdir(parents=True)
    classes = out / 'classes'; classes.mkdir()
    commands = [[javac, '-cp', str(jar), '-d', str(classes), str(ROOT / 'PatchSurvey.java')],
                [java, '-cp', str(jar) + os.pathsep + str(classes), 'PatchSurvey', str(out / 'syntax.json'), *map(str, JAVA_INPUTS)]]
    for index, command in enumerate(commands):
        with (out / f'command-{index}.log').open('w', encoding='utf-8') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    records = json.loads((out / 'syntax.json').read_text(encoding='utf-8'))
    summaries = []
    for source in JAVA_INPUTS:
        selected = [r for r in records if Path(r['source']) == source]
        methods = [r for r in selected if r['kind'] == 'method']
        kinds, calls = Counter(), Counter()
        for record in selected:
            kinds.update(record.get('node_kinds', {})); calls.update(record.get('syntactic_calls', {}))
        summaries.append(dict(source=source.relative_to(REVANCED).as_posix(),
                              methods=len(methods), fields=sum(r['kind'] == 'field' for r in selected),
                              initializers=sum(r['kind'] == 'initializer' for r in selected),
                              synchronized_methods=sum('synchronized' in r['modifiers'] for r in methods),
                              node_kinds=dict(kinds), syntactic_calls=dict(calls)))
    after = {str(p): digest(p) for p in inputs}
    if before != after:
        raise RuntimeError('Investigation input drift')
    write(out / 'summary.json', summaries)
    write(out / 'receipt.json', dict(schema='stateful-translator.syntax-investigation.v1',
        input_sha256=before, commands=commands, source_files=len(JAVA_INPUTS),
        methods=sum(s['methods'] for s in summaries), fields=sum(s['fields'] for s in summaries),
        types_attributed=False, dependency_closure_complete=False, translation_generated=False,
        scope='Syntax facts, including callback bodies. Call spellings are not resolved method identities; nested bodies can contribute to enclosing counts.'))
    write(out / 'artifact-ledger.json', {str(p.relative_to(out)): digest(p) for p in out.rglob('*') if p.is_file()})
    for summary in summaries:
        kinds = summary['node_kinds']
        print(Path(summary['source']).name, 'methods=', summary['methods'], 'fields=', summary['fields'],
              'lambdas=', kinds.get('LAMBDA_EXPRESSION', 0), 'try=', kinds.get('TRY', 0),
              'monitors=', kinds.get('SYNCHRONIZED', 0), 'synchronized_methods=', summary['synchronized_methods'])
    print('Evidence:', out)


if __name__ == '__main__':
    main()
