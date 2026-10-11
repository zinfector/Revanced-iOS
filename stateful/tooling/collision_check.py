"""Read-only main executable class/symbol collision gate for a generated payload."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ipa',type=Path,required=True)
    p.add_argument('--symbols',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    if a.out.exists():raise ValueError('Fresh collision report required')
    sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'adaptive-patcher'))
    from adaptive_image import Image
    with zipfile.ZipFile(a.ipa) as z:
        executable=[name for name in z.namelist() if name.startswith('Payload/') and name.endswith('.app/YouTube')]
        if len(executable)!=1:raise ValueError('Unique main executable required')
        data=z.read(executable[0])
    image=Image(data);sec=next(s for s in image.sections if s['name']=='__objc_classlist')
    classes=set();errors=[]
    for i in range(sec['size']//8):
        try:
            address=image.ptr(sec['va']+i*8);name=image.string(image.ptr(image.ro(address)+24))
            if not name or name in classes:raise ValueError('Empty/duplicate defined class')
            classes.add(name)
        except Exception as ex:errors.append(dict(slot=i,error=str(ex)))
    payload=set()
    for line in a.symbols.read_text().splitlines():
        pieces=line.split()
        if len(pieces)>=3 and pieces[-2].upper()!='U':payload.add(pieces[-1])
    payload_classes={s.split('$_',1)[1] for s in payload if s.startswith('_OBJC_CLASS_$_')}
    definitions=set();offset=32;count=struct.unpack_from('<I',data,16)[0];symtab=None
    for _ in range(count):
        command,size=struct.unpack_from('<II',data,offset)
        if command==2:symtab=struct.unpack_from('<IIII',data,offset+8)
        offset+=size
    if symtab:
        symbols,n,strs,length=symtab
        if symbols+n*16>len(data) or strs+length>len(data):raise ValueError('Invalid symbol/string range')
        for i in range(n):
            string,kind,section,desc,value=struct.unpack_from('<IBBHQ',data,symbols+i*16)
            if kind & 0xe0 or not kind & 0x0e or not string:continue
            if string>=length:raise ValueError('Invalid symbol name offset')
            end=data.find(b'\0',strs+string,strs+length)
            if end<0:raise ValueError('Unterminated symbol')
            definitions.add(data[strs+string:end].decode())
    collisions=dict(classes=sorted(classes&payload_classes),symbols=sorted(definitions&payload))
    report=dict(main_executable=executable[0],main_sha256=hashlib.sha256(data).hexdigest(),uuid=image.uuid,
        payload_symbols_sha256=hashlib.sha256(a.symbols.read_bytes()).hexdigest(),main_class_count=len(classes),
        main_class_slots=sec['size']//8,class_inventory_errors=errors,payload_class_count=len(payload_classes),
        main_defined_symbol_count=len(definitions),main_has_symbol_table=symtab is not None,collisions=collisions,
        main_class_inventory_complete=not errors,
        main_collisions_found=bool(collisions['classes'] or collisions['symbols']),
        scope='Main executable only; embedded/framework images and runtime registration still require review before injection',
        device_verified=False,app_injection_authorized_by_this_report=False)
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n',newline='\n')
    print(json.dumps({k:report[k] for k in ['main_class_count','payload_class_count','main_defined_symbol_count','main_class_inventory_complete','main_collisions_found','scope']},indent=2))


if __name__=='__main__':main()
