"""Rejection checks and an observable IR edit, using a fresh checked delivery."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import sys
from contracts import CONTRACTS
from driver import classes, ledger, run, source, write, PACKAGE
from regions import AdmissionError, hash_graph, verify, walk


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();out=a.out.resolve();inputs=a.input.resolve()
    if out.exists():raise ValueError('Fresh validation output required')
    out.mkdir(parents=True);module=json.loads((inputs/'module.json').read_text())
    rejected=[]
    for name, mutate in [
        ('unknown-call',lambda ns:ns.__setitem__('target','android.unmodeled.Service#run()')),
        ('unsupported-operation',lambda ns:ns.__setitem__('kind','FOR_LOOP')),
        ('raw-source',lambda ns:ns.__setitem__('raw','arbitrary Java')),
        ('type-injection',lambda ns:ns.__setitem__('type','float; injected')),
    ]:
        changed=copy.deepcopy(module);node=next(n for n in walk(changed) if n['kind']=='METHOD_INVOCATION');mutate(node)
        try:verify(changed,CONTRACTS)
        except AdmissionError:rejected.append(name)
        else:raise AssertionError('Unsafe module admitted: '+name)
    changed=copy.deepcopy(module);callback=next(n for n in walk(changed) if n['kind']=='LAMBDA_EXPRESSION' and n['captures'])
    callback['captures'].pop()
    try:verify(changed,CONTRACTS)
    except AdmissionError:rejected.append('missing-capture')
    else:raise AssertionError('Missing capture admitted')
    driver=Path(__file__).with_name('driver.py')
    wrong=subprocess.run([sys.executable,str(driver),'edit','--input',str(inputs/'module.json'),'--out',str(out/'wrong.json'),
                          '--expected-hash','0'*64,'--node','none','--value','250'],capture_output=True,text=True)
    if wrong.returncode==0 or (out/'wrong.json').exists():raise AssertionError('Stale graph edit was accepted')
    rejected.append('stale-graph-edit')
    field=next(f for c in module['classes'] for f in c['fields'] if f['name']=='TOAST_DELAY_MILLISECONDS');node=field['value']
    run([sys.executable,driver,'edit','--input',inputs/'module.json','--out',out/'edited.json','--expected-hash',hash_graph(module),
         '--node',node['id'],'--value','250'],out/'edit.log')
    run([sys.executable,driver,'emit','--input',out/'edited.json','--out',out/'edited-java'],out/'emit.log')
    for path in (inputs/'generated').rglob('*.java'):
        if path.name!='RememberPlaybackSpeedPatch.java':source(out/'edited-java'/path.relative_to(inputs/'generated'),path.read_text())
    source(out/'edited-java/EditProbe.java','''import rv.translated.Simulator;
import app.revanced.extension.youtube.settings.Settings;
import app.revanced.extension.youtube.patches.playback.speed.RememberPlaybackSpeedPatch;
public final class EditProbe {public static void main(String[] ignored){
    Simulator.reset();Settings.REMEMBER_PLAYBACK_SPEED_LAST_SELECTED.value=true;
    RememberPlaybackSpeedPatch.userSelectedPlaybackSpeed(2.0f);Simulator.runThrough(1249);
    if(Settings.PLAYBACK_SPEED_DEFAULT.value!=-2.0f)throw new AssertionError("Saved early");
    Simulator.runThrough(1250);if(Settings.PLAYBACK_SPEED_DEFAULT.value!=2.0f)throw new AssertionError("IR edit did not change code");
    System.out.println("IR-only edit changed save boundary from 1750ms to 1250ms");}}
''')
    classes(out/'edited-java',out/'edited-classes',out/'edit-javac.log')
    run([shutil.which('java'),'-cp',out/'edited-classes','EditProbe'],out/'edit-probe.txt')
    write(out/'receipt.json',dict(rejected=rejected,edit_only_in_ir=True,source_changed=False,
          original_delay_ms=750,edited_delay_ms=250,edited_graph_sha256=hash_graph(json.loads((out/'edited.json').read_text())),
          edit_compiled_and_executed=True,coverage='Admission boundaries and observable edit; not all-input equivalence proof'))
    ledger(out);print('Rejected '+str(len(rejected))+' unsafe/stale inputs; IR-only edit compiled and changed callback timing')


if __name__=='__main__':main()
