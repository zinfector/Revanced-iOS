"""Explicit platform type/operation lowering after identity round-trip validation."""
import copy
import regions

ALIASES={
    'android.icu.text.NumberFormat':'rv.batch.format.NumberFormat',
    'android.icu.text.DecimalFormat':'rv.batch.format.DecimalFormat',
    'android.icu.text.DecimalFormatSymbols':'rv.batch.format.DecimalFormatSymbols',
    'android.icu.text.CompactDecimalFormat':'rv.batch.format.CompactDecimalFormat',
}


def platform_types(module):
    def rec(x):
        if isinstance(x,dict):return {rec(k):(v if k=='origin' else rec(v)) for k,v in x.items()}
        if isinstance(x,list):return [rec(v) for v in x]
        if isinstance(x,str):
            # Only fully qualified type/symbol spellings; literals remain untouched below.
            for old,new in sorted(ALIASES.items(),key=lambda p:-len(p[0])):x=x.replace(old,new)
        return x
    result=rec(copy.deepcopy(module))
    # Literal contents are data, never type names.
    def restore(a,b):
        if isinstance(a,dict):
            if a.get('kind','').endswith('_LITERAL'):b['value']=a.get('value')
            if 'constant' in a:b['constant']=a['constant']
            for k,v in a.items():
                if isinstance(v,(dict,list)) and k!='origin':restore(v,b[rec(k)])
        elif isinstance(a,list):
            for x,y in zip(a,b):restore(x,y)
    restore(module,result)
    return result


def binding_patterns(module):
    """Lower Java pattern declarations for J2ObjC 3.1, which loses their bindings.

    A single-evaluation temporary and short-circuit assignment preserve evaluation
    order and null behavior. Captured pattern bindings are rejected until the
    backend supports their effective-final semantics.
    """
    result=copy.deepcopy(module);serial=0
    def visit(value):
        nonlocal serial
        if isinstance(value,list):return [visit(v) for v in value]
        if not isinstance(value,dict):return value
        if value.get('kind')=='METHOD' and value.get('body'):
            declarations=[];renames={}
            def lower(x):
                nonlocal serial
                if isinstance(x,list):return [lower(v) for v in x]
                if not isinstance(x,dict):return x
                # Nested class methods must own their own temporaries.
                if x.get('kind') in ('CLASS','ENUM','INTERFACE'):return visit(x)
                if x.get('kind')=='INSTANCE_OF' and x.get('getPattern'):
                    binding=x['getPattern']['getVariable'];symbol=binding['symbol']
                    if any(c['symbol']==symbol for n in regions.walk(value['body']) if n['kind']=='LAMBDA_EXPRESSION' for c in n['captures']):raise ValueError('Captured pattern binding is not admitted')
                    serial+=1;name='__rv_pattern_'+str(serial);temp='__rv_pattern_input_'+str(serial)
                    renames[symbol]=name
                    def ref(sym,n,t):return dict(kind='IDENTIFIER',symbol=sym,name=n,type=t,field=False,modifiers=[])
                    def null():return dict(kind='NULL_LITERAL',type='<nulltype>',value=None)
                    binding=copy.deepcopy(binding);binding.update(name=name,modifiers=[],value=null())
                    tempvar=dict(kind='VARIABLE',symbol='local_pattern_input_'+str(serial),name=temp,declared_type='java.lang.Object',type='java.lang.Object',modifiers=[],value=null(),enum_constant=False)
                    declarations.extend([tempvar,binding])
                    target=ref(tempvar['symbol'],temp,'java.lang.Object');bound=ref(symbol,name,binding['declared_type'])
                    assignment=dict(kind='ASSIGNMENT',target=target,value=lower(x['getExpression']),type='java.lang.Object')
                    test=dict(kind='INSTANCE_OF',getExpression=dict(kind='PARENTHESIZED',value=assignment,type='java.lang.Object'),getType=dict(kind='IDENTIFIER',**{'class':binding['declared_type']},type=binding['declared_type']),getPattern=None,type='boolean')
                    cast=dict(kind='TYPE_CAST',cast_type=binding['declared_type'],type=binding['declared_type'],value=target)
                    setbound=dict(kind='ASSIGNMENT',target=bound,value=cast,type=binding['declared_type'])
                    return dict(kind='CONDITIONAL_AND',left=test,right=dict(kind='NOT_EQUAL_TO',left=dict(kind='PARENTHESIZED',value=setbound,type=binding['declared_type']),right=null(),type='boolean'),type='boolean')
                return {k:lower(v) if k not in ('origin','captures') else v for k,v in x.items()}
            body=lower(value['body'])
            def rename(x):
                if isinstance(x,list):return [rename(v) for v in x]
                if not isinstance(x,dict):return x
                if x.get('symbol') in renames:x['name']=renames[x['symbol']]
                return {k:rename(v) if k not in ('origin',) else v for k,v in x.items()}
            body=rename(body);body['statements']=declarations+body['statements']
            value['body']=body
            return value
        return {k:visit(v) for k,v in value.items() if k not in ('fields','methods','nested','initializers')}
    result=visit(result)
    return result
