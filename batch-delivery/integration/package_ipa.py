"""Package the original IPA with the generated core and fresh host, without legacy patcher code."""
import argparse,hashlib,json,plistlib,struct,zipfile
from pathlib import Path

def sha(data):return hashlib.sha256(data).hexdigest()
def image(data):
    if len(data)<32 or struct.unpack_from('<II',data)!= (0xfeedfacf,0x100000c):raise ValueError('Expected thin ARM64 Mach-O')
    count,total=struct.unpack_from('<II',data,16);position=32;first=len(data);loads=[];uuid=None;cryptid=0
    for _ in range(count):
        command,size=struct.unpack_from('<II',data,position)
        if size<8 or position+size>32+total:raise ValueError('Invalid Mach-O command')
        if command==0x19:
            sections=struct.unpack_from('<I',data,position+64)[0]
            for n in range(sections):
                offset=struct.unpack_from('<I',data,position+72+n*80+48)[0]
                if offset:first=min(first,offset)
        if command in (0xc,0xd,0x80000018,0x8000001f):
            rel=struct.unpack_from('<I',data,position+8)[0];loads.append(data[position+rel:position+size].split(b'\0')[0].decode())
        if command==0x1b:uuid=data[position+8:position+24].hex()
        if command==0x2c:cryptid=struct.unpack_from('<I',data,position+16)[0]
        position+=size
    if position!=32+total:raise ValueError('Command size mismatch')
    return dict(count=count,total=total,end=position,first=first,padding=first-position,loads=loads,uuid=uuid,cryptid=cryptid)

def inject(data,name):
    meta=image(data)
    if meta['cryptid']:raise ValueError('Encrypted application executable')
    if name in meta['loads']:raise ValueError('Input already contains this integration')
    text=name.encode()+b'\0';size=(24+len(text)+7)&~7;command=struct.pack('<IIIIII',0xc,size,24,0,0x10000,0x10000)+text;command+=bytes(size-len(command))
    if meta['padding']<size or any(data[meta['end']:meta['end']+size]):raise ValueError('Insufficient zero-filled load-command padding')
    out=bytearray(data);out[meta['end']:meta['end']+size]=command;struct.pack_into('<II',out,16,meta['count']+1,meta['total']+size)
    return bytes(out)

def package(original,artifacts,out,run):
    if out.exists():raise ValueError('Refusing to overwrite an existing IPA')
    app=artifacts/'app';native=artifacts/'native';host=(app/'RVBHost.dylib').read_bytes();core=(native/'TranslatedBatch.dylib').read_bytes();catalog=json.loads((app/'RVBIntegration.json').read_text())
    hr=json.loads((app/'receipt.json').read_text());nr=json.loads((native/'receipt.json').read_text())
    if sha(host)!=hr['host_sha256'] or sha(core)!=hr['core_sha256'] or sha(core)!=nr['payload_sha256']:raise ValueError('Cloud payload hashes do not match receipts')
    hmeta=image(host);cmeta=image(core)
    dependency='@executable_path/Frameworks/TranslatedBatch.dylib'
    if dependency not in hmeta['loads']:raise ValueError('Host dependency is missing')
    for name,m in [('host',hmeta),('core',cmeta)]:
        if m['padding']<16:raise ValueError(name+' has no load-command space for signing')
    # The original IPA remains local and is not uploaded to the workflow.
    original_hash=sha(original.read_bytes())
    if original_hash!=catalog['original_archive_sha256']:raise ValueError('Input IPA does not match the reviewed target')
    with zipfile.ZipFile(original) as source:
        infos=[n for n in source.namelist() if n.startswith('Payload/') and n.count('/')==2 and n.endswith('/Info.plist')]
        if len(infos)!=1:raise ValueError('Expected one root app')
        info_name=infos[0];root=info_name.rsplit('/',1)[0]+'/';info=plistlib.loads(source.read(info_name));executable=root+info['CFBundleExecutable'];binary=source.read(executable)
        if sha(binary)!=catalog['original_executable_sha256'] or image(binary)['uuid']!=catalog['app_uuid']:raise ValueError('Executable identity mismatch')
        if info['CFBundleShortVersionString']!=catalog['app_version']:raise ValueError('Version mismatch')
        modified=inject(binary,'@executable_path/Frameworks/RVBHost.dylib')
        info['CFBundleDisplayName']='YouTube Translated';info['GTMUserAgentID']=info['CFBundleIdentifier'];info['RVBIntegrationVersion']=catalog['version']
        receipt=dict(schema='stateful.batch.ipa.v1',version=catalog['version'],cloud_run=run,original_archive_sha256=original_hash,original_executable_sha256=sha(binary),modified_executable_sha256=sha(modified),core_sha256=sha(core),host_sha256=sha(host),generated_java_files=len(nr['java_files']),legacy_ios_sources_used=False,legacy_ios_payloads_included=False,full_comparison_executed=False,signing='unsigned; SideStore must sign the app and both Frameworks dylibs',debugger=dict(protocol='HTTP GET /state',port=6179,interface='127.0.0.1',authorization='Bearer token copied from settings',read_only=True,compatible_with_legacy_debugger=False),device_verified=False)
        additions={root+'Frameworks/RVBHost.dylib':host,root+'Frameworks/TranslatedBatch.dylib':core,root+'RVBIntegration.json':(app/'RVBIntegration.json').read_bytes(),root+'RVBResources.json':(app/'RVBResources.json').read_bytes(),root+'RVBDelivery.json':(json.dumps(receipt,indent=2)+'\n').encode()}
        out.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(out,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as target:
            for member in source.infolist():
                if member.filename.startswith(root+'PlugIns/') or '/_CodeSignature/' in member.filename or member.filename==root+'embedded.mobileprovision':continue
                if member.filename in additions:raise ValueError('Original archive has integration collision')
                value=modified if member.filename==executable else plistlib.dumps(info,fmt=plistlib.FMT_BINARY) if member.filename==info_name else source.read(member)
                target.writestr(member,value)
            for name,value in additions.items():
                member=zipfile.ZipInfo(name);member.compress_type=zipfile.ZIP_DEFLATED;member.external_attr=(0o100755 if name.endswith('.dylib') else 0o100644)<<16;target.writestr(member,value)
    # One delivery integrity inspection, not a behavior comparison suite.
    with zipfile.ZipFile(out) as result:
        if result.testzip():raise ValueError('Corrupt output archive')
        for name,value in additions.items():
            if sha(result.read(name))!=sha(value):raise ValueError('Packaged artifact mismatch: '+name)
        if '@executable_path/Frameworks/RVBHost.dylib' not in image(result.read(executable))['loads']:raise ValueError('App does not load the fresh host')
        if any(n.endswith('/RVPort.dylib') for n in result.namelist()):raise ValueError('Legacy payload unexpectedly included')
    receipt.update(ipa_sha256=sha(out.read_bytes()),ipa_bytes=out.stat().st_size,zip_integrity_verified=True)
    out.with_suffix('.receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(dict(ipa=str(out),bytes=receipt['ipa_bytes'],sha256=receipt['ipa_sha256'],legacy_ios_payloads_included=False),indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--run',required=True);a=p.parse_args();package(a.original.resolve(),a.artifacts.resolve(),a.out.resolve(),a.run)
