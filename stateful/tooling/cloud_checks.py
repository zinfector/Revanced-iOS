"""Independently attribute retained source, regenerate policy and replay JVM oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
from driver import classes, run, write, ledger, PACKAGE
from contracts import CONTRACTS
from regions import annotate, emit_java, semantic_form, verify


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--gson',type=Path,required=True)
    a=p.parse_args();inputs=a.input.resolve();out=a.out.resolve();gson=a.gson.resolve()
    if out.exists():raise ValueError('Fresh cloud validation output required')
    out.mkdir(parents=True)
    if sha(gson)!='dd0ce1b55a3ed2080cb70f9c655850cda86c206862310009dcb5e5c95265a5e0':raise ValueError('Gson tool hash mismatch')
    for path,value in json.loads((inputs/'delivery-ledger.json').read_text()).items():
        if sha(inputs/path)!=value:raise ValueError('Delivery drift: '+path)
    retained=inputs/'retained/RememberPlaybackSpeedPatch.java'
    receipt=json.loads((inputs/'generation-receipt.json').read_text())
    original_sha=next(v for k,v in receipt['input_sha256'].items() if k.endswith('RememberPlaybackSpeedPatch.java'))
    if sha(retained)!=original_sha:raise ValueError('Retained source identity differs from upstream receipt')
    expected=retained.read_text().replace('System.currentTimeMillis()','rv.translated.PlatformClock.wallMillis()')
    if expected!=(inputs/'original'/(PACKAGE+'/RememberPlaybackSpeedPatch.java')).read_text():raise ValueError('Oracle contains more than clock seam')
    classes(inputs/'original',out/'original-classes',out/'original-javac.log')
    compiler=out/'exporter';compiler.mkdir();tool=Path(__file__).with_name('ExportPatch.java')
    run([shutil.which('javac'),'-cp',gson,'-d',compiler,tool],out/'exporter-javac.log')
    run([shutil.which('java'),'-cp',str(gson)+os.pathsep+str(compiler),'ExportPatch',out/'attributed.json',out/'original-classes','',retained],out/'attribution.log')
    attributed=json.loads((out/'attributed.json').read_text());facts=json.loads((inputs/'settings.json').read_text())
    attributed['external_fields']={f['symbol']:f for f in facts};attributed=annotate(attributed,CONTRACTS);verify(attributed,CONTRACTS)
    module=json.loads((inputs/'module.json').read_text())
    if semantic_form(attributed,CONTRACTS)!=semantic_form(module,CONTRACTS):raise ValueError('Delivered IR differs from freshly attributed original')
    for name, text in emit_java(attributed,CONTRACTS).items():
        for folder in ('generated','production-java'):
            if text!=(inputs/folder/name).read_text():raise ValueError('Native input policy not regenerated from source IR')
    classes(inputs/'generated',out/'generated-classes',out/'generated-javac.log')
    traces={}
    for variant in ('original','generated'):
        run([shutil.which('java'),'-cp',out/(variant+'-classes'),'rv.translated.TraceHarness'],out/(variant+'-trace.txt'))
        traces[variant]=(out/(variant+'-trace.txt')).read_text()
    if traces['original']!=traces['generated'] or traces['original']!=(inputs/'original-trace.txt').read_text():raise ValueError('Fresh JVM oracle differs')
    write(out/'receipt.json',dict(upstream_source_sha256=original_sha,frontend_sha256=sha(tool),gson_sha256=sha(gson),
        original_source_freshly_attributed=True,source_ir_semantics_equal=True,source_ir_java_regenerated_equal=True,
        production_policy_body_is_same_generated_code=True,jvm_cases=len(traces['original'].splitlines()),fresh_jvm_traces_equal=True,
        scope='Source/IR regeneration and JVM traces; application bindings remain separately reviewed'))
    ledger(out);print('Original source freshly attributed; IR regenerated; JVM oracle matched; production policy is identical generated body')


if __name__=='__main__':main()
