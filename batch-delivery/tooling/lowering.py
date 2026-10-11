"""Explicit platform type/operation lowering after identity round-trip validation."""
import copy

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
