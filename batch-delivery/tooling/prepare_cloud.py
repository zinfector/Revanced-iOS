"""Stage only batch inputs in an isolated worktree; never stage prior IPA outputs."""
import argparse
import shutil
from pathlib import Path


ROOT=Path(__file__).resolve().parent
def prepare(worktree,generation):
    dest=worktree/'batch-delivery';dest.mkdir(exist_ok=True)
    shutil.copytree(generation/'portable-src',dest/'java',dirs_exist_ok=True)
    shutil.copytree(ROOT/'native',dest/'native',dirs_exist_ok=True)
    for name in ('native_build.py',):shutil.copy2(ROOT/name,dest/name)
    shutil.copytree(ROOT/'tests',dest/'tests',dirs_exist_ok=True)
    validation=generation.parent/'validation-current'
    if not validation.exists():raise ValueError('Run the latest source/native fixture validation before staging')
    shutil.copy2(validation/'oracle-trace.txt',dest/'original-trace.txt')
    shutil.copy2(validation/'receipt.json',dest/'behavior-receipt.json')
    shutil.copy2(generation/'generation-receipt.json',dest/'generation-receipt.json')
    shutil.copytree(generation/'retained',dest/'retained',dirs_exist_ok=True)
    tooling=dest/'tooling';tooling.mkdir(exist_ok=True)
    for p in [*ROOT.glob('*.py'),*ROOT.glob('*.java')]:shutil.copy2(p,tooling/p.name)
    workflow=worktree/'.github/workflows/stateful-batch.yml'
    workflow.write_text(WORKFLOW,encoding='utf-8')


WORKFLOW='''name: Larger source translated batch
on:
  push:
    branches: [feat/stateful-batch-0.2]
permissions:
  contents: read
jobs:
  batch:
    runs-on: macos-15
    timeout-minutes: 55
    env:
      DEVELOPER_DIR: /Applications/Xcode_16.4.app/Contents/Developer
    steps:
      - uses: actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803
        with:
          persist-credentials: false
      - uses: actions/setup-java@cf277c60eb25467037889841efdb72551f06f6c3
        with:
          distribution: temurin
          java-version: '21'
      - uses: actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1
        with:
          python-version: '3.14'
      - uses: actions/cache@0057852bfaa89a56745cba8c7296529d2fc39830
        with:
          path: j2objc
          key: j2objc-1671e9a-xcode164-jdk21-full-host-ios-v1
          restore-keys: j2objc-1671e9a-xcode164-jdk21-core-host-ios-v2
      - name: Build pinned translator and complete runtime
        run: |
          set -o pipefail
          mkdir -p batch-output
          if [ ! -d j2objc/.git ]; then
            git clone --depth 1 --branch 3.1 https://github.com/google/j2objc.git j2objc
          fi
          test "$(git -C j2objc rev-parse HEAD)" = 1671e9afb72a16126d448f468db3e582358fba47
          make -C j2objc -j3 translator_dist 2>&1 | tee batch-output/vendor-translator.log
          make -C j2objc/jre_emul -j3 J2OBJC_ARCHS="macosx macosx64 iphone64" dist_includes ../dist/lib/macosx/libjre_emul.a ../dist/lib/iphone/libjre_emul.a ../dist/lib/macosx/libjson.a ../dist/lib/iphone/libjson.a 2>&1 | tee batch-output/vendor-runtime.log
      - name: Translate, execute and compile ARM64 iOS
        run: python batch-delivery/native_build.py --j2objc-home j2objc/dist --input batch-delivery/java --native batch-delivery/native --out batch-output/native
      - uses: actions/upload-artifact@b7c566a772e6b6bfb58ed0dc250532a479d7789f
        if: always()
        with:
          name: larger-translated-batch
          path: batch-output
          retention-days: 14
'''


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--worktree',type=Path,required=True);p.add_argument('--generation',type=Path,required=True);a=p.parse_args();prepare(a.worktree.resolve(),a.generation.resolve())
