"""Validate supplied PNG assets and configure legacy iOS bundle icons without resizing."""
from pathlib import Path
import struct
import zlib
from macho import PatchError

def png(path, dimensions=None):
    data=Path(path).read_bytes()
    if len(data)>5*1024*1024 or data[:8]!=b'\x89PNG\r\n\x1a\n':raise PatchError('Expected a PNG asset of at most 5 MB')
    pos=8;compressed=bytearray();size=None;channels=None;ended=False
    while pos<len(data):
        if pos+12>len(data):raise PatchError('Truncated PNG chunk')
        length=struct.unpack_from('>I',data,pos)[0];kind=data[pos+4:pos+8];end=pos+8+length
        if end+4>len(data) or zlib.crc32(data[pos+4:end])!=struct.unpack_from('>I',data,end)[0]:raise PatchError('Invalid PNG chunk/CRC')
        payload=data[pos+8:end]
        if kind==b'IHDR':
            if size or pos!=8 or length!=13:raise PatchError('Invalid PNG header')
            w,h,depth,color,compression,filtering,interlace=struct.unpack('>2I5B',payload)
            if not 1<=w<=2048 or not 1<=h<=2048 or depth!=8 or color not in (2,6) or compression or filtering or interlace:
                raise PatchError('Use an 8-bit RGB/RGBA, non-interlaced PNG up to 2048x2048')
            size=(w,h);channels=3 if color==2 else 4
        elif kind==b'IDAT':compressed.extend(payload)
        elif kind==b'IEND':
            if length or end+4!=len(data):raise PatchError('Invalid PNG end')
            ended=True;break
        elif kind[:1].isupper():raise PatchError('Unsupported critical PNG chunk')
        pos=end+4
    if not size or not ended or not compressed:raise PatchError('Incomplete PNG')
    if dimensions and size!=dimensions:raise PatchError(f'Expected PNG dimensions {dimensions}, got {size}')
    expected=(size[0]*channels+1)*size[1]
    decoder=zlib.decompressobj()
    try: pixels=decoder.decompress(bytes(compressed),expected+1)
    except zlib.error as ex:raise PatchError('Invalid PNG image data') from ex
    if not decoder.eof or decoder.unused_data or len(pixels)!=expected:raise PatchError('Invalid PNG pixel data size')
    row=size[0]*channels+1
    if any(pixels[p]>4 for p in range(0,len(pixels),row)):raise PatchError('Invalid PNG row filter')
    return data

def prepare(info, assets):
    allowed={'header_image','icon_120','icon_180','icon_152'}
    if set(assets)-allowed:raise PatchError('Unknown branding asset')
    result={}
    if assets.get('header_image'):result['RVHeader.png']=png(assets['header_image'])
    phone=bool(assets.get('icon_120') or assets.get('icon_180'))
    if phone:
        if not assets.get('icon_120') or not assets.get('icon_180'):raise PatchError('Phone branding requires both 120x120 and 180x180 icons')
        result['RVAppIcon@2x.png']=png(assets['icon_120'],(120,120))
        result['RVAppIcon@3x.png']=png(assets['icon_180'],(180,180))
        info['CFBundleIcons']={'CFBundlePrimaryIcon':{'CFBundleIconFiles':['RVAppIcon']}}
    if assets.get('icon_152'):
        result['RVPadIcon@2x~ipad.png']=png(assets['icon_152'],(152,152))
        info['CFBundleIcons~ipad']={'CFBundlePrimaryIcon':{'CFBundleIconFiles':['RVPadIcon']}}
    return result
