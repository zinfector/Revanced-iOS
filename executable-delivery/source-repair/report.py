"""Read a generated payload's published JSON through the existing USB debugger.

Uses only debug.status, debug.images, debug.read and debug.job. Does not call
native functions, initialize policies, change settings or install hooks.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
import sys
import time
import uuid


def report(native, udid=None, port=49629):
    from .package import injector
    root=Path(__file__).resolve().parents[3]
    sys.path.insert(0,str(root/'patcher'))
    import live_debugger as dbg
    native=Path(native).resolve()
    receipt=json.loads((native/'receipt.json').read_text(encoding='utf-8'))
    registration=receipt['diagnostic_report']
    if registration['schema']!='installer.report-buffer.v1' or registration['capacity']!=32768:
        raise ValueError('Unknown published-report protocol')
    raw=(native/receipt['payload']).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=receipt['payload_sha256']:raise ValueError('Payload differs from build receipt')
    image=injector().MachO(raw)
    command=next(c for c in image.commands if c.kind==2)
    _,_,offset,count,strings,length=struct.unpack('<6I',command.raw)
    table=raw[strings:strings+length];symbols={}
    required={registration[k] for k in ('generation_symbol','length_symbol','buffer_symbol')}
    for i in range(count):
        index,kind,_,_,address=struct.unpack_from('<IBBHQ',raw,offset+16*i)
        name=table[index:table.find(b'\0',index)].decode('utf-8')
        if kind&0xe==0xe and name in required:symbols[name]=address
    if set(symbols)!=required:raise ValueError('Published-report symbols missing')
    connection=dbg.Connection(argparse.Namespace(udid=udid,port=port))
    status=connection.once('debug.status')
    if not status.get('ok') or not status['data'].get('enabled'):raise ValueError('Live debugging is disabled')
    nonce=status['data']['process_nonce']
    def job(operation,args):
        identifier=str(uuid.uuid4());response=connection.once(operation,args,identifier,nonce)
        for _ in range(30):
            if not response.get('ok') or response.get('process_nonce')!=nonce:raise ValueError('Debugger rejected request or process changed')
            data=response['data']
            if data['state'] in dbg.TERMINAL:
                if data['state']!='completed' or not data['result'].get('ok'):raise ValueError('Debugger read failed: '+str(data['result'].get('error','unknown')))
                return data['result']
            time.sleep(.1);response=connection.once('debug.job',{'job_id':identifier},nonce=nonce)
        raise TimeoutError('Debugger job exceeded bounded wait')
    images=job('debug.images',{})['images']
    matching=[r for r in images if r['uuid'].replace('-','').lower()==image.uuid]
    if len(matching)!=1:raise ValueError('Exact freshly compiled payload is not loaded')
    loaded=matching[0]
    def read(name,length):
        return base64.b64decode(job('debug.read',{'image_uuid':loaded['uuid'],'offset':hex(symbols[name]),'length':length})['bytes'])
    for _ in range(3):
        generation=int.from_bytes(read(registration['generation_symbol'],8),'little')
        if not generation or generation&1:continue
        length=int.from_bytes(read(registration['length_symbol'],4),'little')
        if not 0<length<registration['capacity']:raise ValueError('Invalid snapshot length')
        data=read(registration['buffer_symbol'],length)
        after=int.from_bytes(read(registration['generation_symbol'],8),'little')
        if generation!=after or after&1:continue
        return {'schema':'installer.live-published-report.v1','read_only':True,'process_nonce':nonce,
                'payload_uuid':loaded['uuid'],'payload_sha256':receipt['payload_sha256'],
                'generation':generation,'report':json.loads(data.decode('utf-8'))}
    raise RuntimeError('Report changed during capture; request again')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True)
    p.add_argument('--udid');p.add_argument('--port',type=int,default=49629);p.add_argument('--out',type=Path)
    a=p.parse_args();result=report(a.native,a.udid,a.port);text=json.dumps(result,indent=2)+'\n'
    if a.out:
        with a.out.open('x',encoding='utf-8') as stream:stream.write(text)
    print(text)
