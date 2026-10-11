"""Compare independently attributed programs, ignoring spelling and source locations."""
import json
import re
import regions


def semantic_form(module):
    locals_by_symbol={}
    for n in regions.walk(module):
        if n['kind']=='VARIABLE' and n['symbol'].startswith('local_'):
            locals_by_symbol.setdefault(n['symbol'],'v'+str(len(locals_by_symbol)))
    skip={'id','origin','effects','imports','fields','methods','nested','initializers','types_attributed','schema'}
    def rec(x):
        if isinstance(x,list):return [rec(v) for v in x]
        if not isinstance(x,dict):
            if isinstance(x,str):return locals_by_symbol.get(x,re.sub(r'capture#\d+','capture',x))
            return x
        k=x.get('kind')
        if k=='PARENTHESIZED':return rec(x['value'])
        if k in ('IDENTIFIER','MEMBER_SELECT'):
            if x.get('class_literal'):return dict(kind='class_literal',value=rec(x['class_literal']))
            if x.get('array_length'):return dict(kind='array_length',value=rec(x['array_length']))
            if x.get('class'):return dict(kind='type_reference',name=x['class'])
            if x.get('type_variable'):return dict(kind='type_variable',name=x['type_variable'])
            if x.get('this'):return dict(kind='this',type=x['type'])
            if 'constant' in x and x.get('symbol') not in declared_fields:
                return dict(kind='literal',type=x['type'],value=x['constant'])
            return dict(kind='reference',symbol=rec(x['symbol']),type=rec(x['type']),receiver=rec(x.get('receiver')))
        if k in regions.LITERALS:return dict(kind='literal',type=x['type'],value=x.get('value'))
        d={key:rec(v) for key,v in x.items() if key not in skip}
        if k in ('IF','WHILE_LOOP','FOR_LOOP','ENHANCED_FOR_LOOP','DO_WHILE_LOOP'):
            for key in ('then','else','body','getStatement'):
                v=d.get(key)
                if isinstance(v,dict) and v.get('kind')!='BLOCK':d[key]=dict(kind='BLOCK',static=False,statements=[v])
        if k in ('CLASS','ENUM','INTERFACE') and k=='ENUM':d['modifiers']=[v for v in d['modifiers'] if v not in ('final','abstract')]
        return d
    declared_fields={n.get('symbol') for n in regions.walk(module) if n['kind']=='VARIABLE'}
    return rec(module)


def first_difference(a,b,path='$'):
    if type(a)!=type(b):return dict(path=path,original=a,regenerated=b)
    if isinstance(a,dict):
        if a.keys()!=b.keys():return dict(path=path,original_keys=sorted(a),regenerated_keys=sorted(b))
        for key in sorted(a):
            d=first_difference(a[key],b[key],path+'.'+key)
            if d:return d
    elif isinstance(a,list):
        if len(a)!=len(b):return dict(path=path,original_length=len(a),regenerated_length=len(b))
        for i,(x,y) in enumerate(zip(a,b)):
            d=first_difference(x,y,path+'['+str(i)+']')
            if d:return d
    elif a!=b:return dict(path=path,original=a,regenerated=b)


def check(original,regenerated):
    a,b=semantic_form(original),semantic_form(regenerated)
    diff=first_difference(a,b)
    if diff:raise ValueError('Independent re-attribution differs: '+json.dumps(diff,ensure_ascii=True))
    return dict(independently_reattributed=True,semantic_form_sha256=regions.graph_hash(a))
