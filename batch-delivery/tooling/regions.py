"""Attributed structured Java dialect v3. Emission has no executable text escape."""
import copy
import hashlib
import json
import re

SCHEMA='editable-code.patch.regions.v3'
NAME=re.compile(r'[A-Za-z_$][A-Za-z0-9_$]*\Z')
TYPE=re.compile(r'[A-Za-z0-9_$.<>?, &\[\]]+\Z')
MODS={'public','private','protected','static','final','abstract','volatile','synchronized','transient','strictfp','default'}
OPS={'PLUS':'+','MINUS':'-','MULTIPLY':'*','DIVIDE':'/','REMAINDER':'%','EQUAL_TO':'==','NOT_EQUAL_TO':'!=',
     'LESS_THAN':'<','LESS_THAN_EQUAL':'<=','GREATER_THAN':'>','GREATER_THAN_EQUAL':'>=','CONDITIONAL_AND':'&&',
     'CONDITIONAL_OR':'||','AND':'&','OR':'|','XOR':'^','LEFT_SHIFT':'<<','RIGHT_SHIFT':'>>','UNSIGNED_RIGHT_SHIFT':'>>>'}
UNARY={'UNARY_MINUS':'-','UNARY_PLUS':'+','LOGICAL_COMPLEMENT':'!','BITWISE_COMPLEMENT':'~',
       'PREFIX_INCREMENT':'++','PREFIX_DECREMENT':'--','POSTFIX_INCREMENT':'++','POSTFIX_DECREMENT':'--'}
LITERALS={'BOOLEAN_LITERAL','INT_LITERAL','LONG_LITERAL','FLOAT_LITERAL','DOUBLE_LITERAL','STRING_LITERAL','NULL_LITERAL','CHAR_LITERAL'}
KINDS={*OPS,*UNARY,*LITERALS,'CLASS','ENUM','INTERFACE','METHOD','BLOCK','VARIABLE','RETURN','IF','IDENTIFIER','MEMBER_SELECT',
 'METHOD_INVOCATION','PARENTHESIZED','EXPRESSION_STATEMENT','ASSIGNMENT','CONDITIONAL_EXPRESSION','LAMBDA_EXPRESSION',
 'TRY','CATCH','SYNCHRONIZED','NEW_CLASS','THROW','TYPE_CAST','WHILE_LOOP','FOR_LOOP','ENHANCED_FOR_LOOP','DO_WHILE_LOOP',
 'BREAK','CONTINUE','EMPTY_STATEMENT','ARRAY_ACCESS','NEW_ARRAY','PARAMETERIZED_TYPE','PRIMITIVE_TYPE','ARRAY_TYPE',
 'TYPE_PARAMETER','EXTENDS_WILDCARD','SUPER_WILDCARD','UNBOUNDED_WILDCARD','UNION_TYPE','INSTANCE_OF','BINDING_PATTERN',
 'SWITCH','SWITCH_EXPRESSION','CASE','DEFAULT_CASE_LABEL','CONSTANT_CASE_LABEL','YIELD','MEMBER_REFERENCE','LABELED_STATEMENT',
 'PLUS_ASSIGNMENT','MINUS_ASSIGNMENT','MULTIPLY_ASSIGNMENT','DIVIDE_ASSIGNMENT','REMAINDER_ASSIGNMENT','AND_ASSIGNMENT','OR_ASSIGNMENT','XOR_ASSIGNMENT','LEFT_SHIFT_ASSIGNMENT','RIGHT_SHIFT_ASSIGNMENT','UNSIGNED_RIGHT_SHIFT_ASSIGNMENT'}


def walk(value):
    if isinstance(value,dict):
        if 'kind' in value:yield value
        for key in sorted(value):
            v=value[key]
            if key not in ('origin','captures','fields','methods','nested','initializers'):yield from walk(v)
    elif isinstance(value,list):
        for v in value:yield from walk(v)


def graph_hash(module):return hashlib.sha256(json.dumps(module,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def require(ok,msg):
    if not ok:raise ValueError(msg)
def safe_type(t):require(TYPE.fullmatch(t),'Unsupported type '+t);return t
def mods(values):require(set(values)<=MODS,'Unsupported modifiers');return ' '.join(values)


def verify(module,contracts,external_fields):
    require(module['schema']==SCHEMA and module['types_attributed'],'Attributed v3 module required')
    calls={n['symbol'] for n in walk(module) if n['kind']=='METHOD'}
    fields={n['symbol'] for n in walk(module) if n['kind']=='VARIABLE' and '#' in n.get('symbol','')}
    # These members are defined by the Java language, not an external adapter.
    for c in walk(module):
        if c['kind'] not in ('CLASS','ENUM','INTERFACE'):continue
        if c['kind']=='ENUM':
            calls.update({c['name']+'#values()',c['name']+'#valueOf(java.lang.String)'})
        if c['kind']!='INTERFACE' and not any(m.get('constructor') for m in c['members']):
            calls.add(c['name']+'#<init>()')
    unresolved=[]
    for n in walk(module):
        require(n['kind'] in KINDS and not n.get('unsupported'),'Unsupported '+n['kind']+' at '+str(n.get('origin')))
        require('raw' not in n and 'text' not in n,'Executable source escape')
        if n.get('modifiers'):mods(n['modifiers'])
        for key in ('type','declared_type','result'):
            if key in n and n[key] and '<anonymous' not in n[key]:safe_type(re.sub(r'capture#\d+','capture',n[key]))
        if n['kind'] in ('METHOD_INVOCATION','NEW_CLASS'):
            target=n['target']
            if target not in calls and target not in contracts and not n.get('anonymous'):unresolved.append(dict(target=target,origin=n.get('origin')))
        if n.get('field') and not n.get('array_length') and n.get('symbol') not in fields and 'constant' not in n and n['symbol'] not in external_fields:
            unresolved.append(dict(field=n['symbol'],origin=n.get('origin')))
    require(not unresolved,'Unresolved operation frontier: '+json.dumps(unresolved))
    return dict(classes=sum(n['kind'] in ('CLASS','ENUM','INTERFACE') for n in walk(module)),methods=len(calls),fields=len(fields))


def annotate(module,contracts):
    result=copy.deepcopy(module)
    for n in walk(result):
        k=n['kind'];effect=[]
        if k in ('METHOD_INVOCATION','NEW_CLASS'):effect=contracts.get(n['target'],{}).get('effects',['call.internal'])
        elif k in ('ASSIGNMENT','PLUS_ASSIGNMENT','MINUS_ASSIGNMENT') or k in UNARY and 'CREMENT' in k:effect=['memory.write']
        elif n.get('field'):effect=['memory.volatile.read' if 'volatile' in n.get('modifiers',[]) else 'memory.read']
        elif k=='ARRAY_ACCESS':effect=['memory.array.read','exception.bounds']
        elif k=='SYNCHRONIZED':effect=['monitor.enter','monitor.exit_on_all_edges']
        elif k=='LAMBDA_EXPRESSION':effect=['closure.allocate']
        elif k=='TRY':effect=['exception.region']
        n['effects']=effect
    return result


def literal(n):
    value=n.get('value');typ=n['type']
    if value is None:return 'null'
    if typ=='boolean':require(type(value) is bool,'Boolean literal required');return str(value).lower()
    if n['kind']=='CHAR_LITERAL':return "'"+json.dumps(value,ensure_ascii=True)[1:-1].replace("'","\\'")+"'"
    if typ=='java.lang.String':return json.dumps(value,ensure_ascii=True)
    if typ=='long':require(re.fullmatch('-?[0-9]+',str(value)),'Integer required');return str(value)+'L'
    require(type(value) in (float,int),'Numeric literal required')
    return str(value)+('f' if typ=='float' else 'd' if typ=='double' else '')


def emit_java(module,contracts,external_fields):
    verify(module,contracts,external_fields)
    ownfields={n.get('symbol') for n in walk(module) if n['kind']=='VARIABLE'}
    def expr(n):
        k=n['kind']
        if k in LITERALS:return literal(n)
        if k in ('IDENTIFIER','MEMBER_SELECT'):
            if n.get('class_literal'):return expr(n['class_literal'])+'.class'
            if n.get('array_length'):return expr(n['array_length'])+'.length'
            if n.get('this'):return 'this'
            if 'constant' in n and n.get('symbol') not in ownfields:
                return literal(dict(kind='CHAR_LITERAL' if n['type']=='char' else 'STRING_LITERAL',type=n['type'],value=n['constant']))
            if n.get('field'):
                if n.get('name')=='length' and n.get('receiver'):return expr(n['receiver'])+'.length'
                if k=='IDENTIFIER':return n['name']
                if 'static' in n['modifiers']:return n['owner']+'.'+n['name']
                return (expr(n['receiver']) if n.get('receiver') else 'this')+'.'+n['name']
            if 'symbol' in n:return n['name']
            if 'class' in n:return n['class']
            if 'type_variable' in n:return n['type_variable']
            raise ValueError('Unmodeled reference '+str(n))
        if k in OPS:return '('+expr(n['left'])+' '+OPS[k]+' '+expr(n['right'])+')'
        if k in UNARY:return (expr(n['value'])+UNARY[k] if k.startswith('POSTFIX') else UNARY[k]+expr(n['value'])) if 'CREMENT' in k else '('+UNARY[k]+expr(n['value'])+')'
        if k=='PARENTHESIZED':return '('+expr(n['value'])+')'
        if k=='ASSIGNMENT':return expr(n['target'])+' = '+expr(n['value'])
        if k.endswith('_ASSIGNMENT'):return expr(n['getVariable'])+' '+OPS[k[:-11]]+'= '+expr(n['getExpression'])
        if k=='METHOD_INVOCATION':
            args=', '.join(expr(a) for a in n['arguments'])
            if contracts.get(n['target'],{}).get('emit_call'):
                contract=contracts[n['target']]
                if contract.get('receiver_first'):
                    require(n.get('receiver') is not None,'Receiver-first operation needs a receiver')
                    args=expr(n['receiver'])+(', '+args if args else '')
                return contract['emit_call']+'('+args+')'
            if n['name'] in ('<init>','super','this'):
                require(n.get('constructor_selector') in ('super','this'),'Explicit constructor selector required')
                return n['constructor_selector']+'('+args+')'
            if not n['static'] and not n.get('receiver'):return n['name']+'('+args+')'
            owner=n['owner'] if n['static'] else expr(n['receiver']) if n.get('receiver') else 'this'
            return owner+'.'+n['name']+'('+args+')'
        if k=='LAMBDA_EXPRESSION':
            params=', '.join(safe_type(p['declared_type'])+' '+p['name'] for p in n['parameters'])
            return '('+params+') -> '+(stmt(n['body'],0) if n['body']['kind']=='BLOCK' else expr(n['body']))
        if k=='NEW_CLASS':
            text='new '+expr(n['constructed_type'])+'('+', '.join(expr(a) for a in n['arguments'])+')'
            require(not n.get('enclosing'),'Qualified allocation is not yet admitted')
            return text+(class_body(n['anonymous'],0) if n.get('anonymous') else '')
        if k=='TYPE_CAST':return '(('+safe_type(n['cast_type'])+')'+expr(n['value'])+')'
        if k=='CONDITIONAL_EXPRESSION':return '('+expr(n['condition'])+' ? '+expr(n['then'])+' : '+expr(n['else'])+')'
        if k=='ARRAY_ACCESS':return expr(n['getExpression'])+'['+expr(n['getIndex'])+']'
        if k=='NEW_ARRAY':
            if n.get('getType'):
                typ=expr(n['getType']);suffix=''
                while typ.endswith('[]'):typ=typ[:-2];suffix+='[]'
                return 'new '+typ+(''.join('['+expr(x)+']' for x in n['getDimensions']) or '[]')+suffix+(' {'+', '.join(expr(x) for x in n['getInitializers'])+'}' if n.get('getInitializers') is not None else '')
            return '{'+', '.join(expr(x) for x in n['getInitializers'])+'}'
        if k=='PRIMITIVE_TYPE':return n['getPrimitiveTypeKind'].lower()
        if k=='PARAMETERIZED_TYPE':return expr(n['getType'])+'<'+', '.join(expr(x) for x in n['getTypeArguments'])+'>'
        if k=='ARRAY_TYPE':return expr(n['getType'])+'[]'
        if k=='TYPE_PARAMETER':return n['getName']+(' extends '+' & '.join(expr(x) for x in n['getBounds']) if n.get('getBounds') else '')
        if k.endswith('_WILDCARD'):return '?'+(' extends '+expr(n['getBound']) if k=='EXTENDS_WILDCARD' else ' super '+expr(n['getBound']) if k=='SUPER_WILDCARD' else '')
        if k=='UNION_TYPE':return ' | '.join(expr(x) for x in n['getTypeAlternatives'])
        if k=='INSTANCE_OF':return '('+expr(n['getExpression'])+' instanceof '+(expr(n['getPattern']) if n.get('getPattern') else expr(n['getType']))+')'
        if k=='BINDING_PATTERN':p=n['getVariable'];return decl(p)
        if k=='SWITCH_EXPRESSION':return 'switch ('+expr(n['getExpression'])+') {'+'\n'.join(stmt(x,1) for x in n['getCases'])+'}'
        if k=='MEMBER_REFERENCE':return expr(n['getQualifierExpression'])+'::'+('new' if n['getMode']=='NEW' else n['getName'])
        raise ValueError('Expression not admitted: '+k)
    def decl(n):
        typ=n['declared_type']
        if ' | ' in typ:require(TYPE.fullmatch(typ.replace('|',' ')),'Unsupported union');return mods(n['modifiers'])+' '+typ+' '+n['name']
        return (mods(n['modifiers'])+' ' if n['modifiers'] else '')+safe_type(typ)+' '+n['name']
    def variable(n):return decl(n)+(' = '+expr(n['value']) if n.get('value') else '')
    def block(n,depth):return '{\n'+''.join(stmt(x,depth+1)+'\n' for x in n['statements'])+'    '*depth+'}'
    def controlled(n,depth):return block(n,depth) if n['kind']=='BLOCK' else '{\n'+stmt(n,depth+1)+'\n'+'    '*depth+'}'
    def stmt(n,depth):
        pad='    '*depth;k=n['kind']
        if k=='BLOCK':return block(n,depth)
        if k=='VARIABLE':return pad+variable(n)+';'
        if k=='EXPRESSION_STATEMENT':return pad+expr(n['value'])+';'
        if k=='RETURN':return pad+'return'+(' '+expr(n['value']) if n.get('value') else '')+';'
        if k=='IF':return pad+'if ('+expr(n['condition'])+') '+controlled(n['then'],depth)+(' else '+controlled(n['else'],depth) if n.get('else') else '')
        if k=='TRY':
            text=pad+'try'+(' ('+'; '.join(variable(x) if x['kind']=='VARIABLE' else expr(x) for x in n['resources'])+')' if n.get('resources') else '')+' '+block(n['body'],depth)
            for c in n['catches']:text+=' catch ('+decl(c['parameter'])+') '+block(c['body'],depth)
            if n.get('finally'):text+=' finally '+block(n['finally'],depth)
            return text
        if k=='SYNCHRONIZED':return pad+'synchronized ('+expr(n['lock'])+') '+block(n['body'],depth)
        if k=='THROW':return pad+'throw '+expr(n['value'])+';'
        if k=='WHILE_LOOP':return pad+'while ('+expr(n['condition'])+') '+controlled(n['body'],depth)
        if k=='FOR_LOOP':
            init=n['getInitializer'];parts=[]
            for i,x in enumerate(init):parts.append(variable(x) if x['kind']=='VARIABLE' and i==0 else x['name']+(' = '+expr(x['value']) if x.get('value') else '') if x['kind']=='VARIABLE' else expr(x['value']))
            return pad+'for ('+', '.join(parts)+'; '+(expr(n['getCondition']) if n.get('getCondition') else '')+'; '+', '.join(expr(x['value']) for x in n['getUpdate'])+') '+controlled(n['getStatement'],depth)
        if k=='ENHANCED_FOR_LOOP':return pad+'for ('+decl(n['getVariable'])+' : '+expr(n['getExpression'])+') '+controlled(n['getStatement'],depth)
        if k=='DO_WHILE_LOOP':return pad+'do '+controlled(n['getStatement'],depth)+' while ('+expr(n['getCondition'])+');'
        if k in ('BREAK','CONTINUE'):return pad+k.lower()+(' '+n['getLabel'] if n.get('getLabel') else '')+';'
        if k=='EMPTY_STATEMENT':return pad+';'
        if k=='SWITCH':return pad+'switch ('+expr(n['getExpression'])+') {\n'+'\n'.join(stmt(c,depth+1) for c in n['getCases'])+'\n'+pad+'}'
        if k=='CASE':
            labels=n.get('getExpressions') or []
            label='case '+', '.join(x['name'] if x.get('enum_constant') else expr(x) for x in labels) if labels else 'default'
            if n['getCaseKind']=='RULE':return pad+label+' -> '+(stmt(n['getBody'],depth) if n['getBody']['kind'] in ('BLOCK','THROW') else expr(n['getBody'])+';')
            return pad+label+':\n'+''.join(stmt(x,depth+1)+'\n' for x in n['getStatements'])
        if k=='YIELD':return pad+'yield '+expr(n['getValue'])+';'
        if k=='LABELED_STATEMENT':return pad+n['getLabel']+': '+stmt(n['getStatement'],depth)
        raise ValueError('Statement not admitted: '+k)
    def method(n,cls,depth):
        head='    '*depth+mods(n['modifiers'])+' '
        if n['type_parameters']:head+='<'+', '.join(expr(x) for x in n['type_parameters'])+'> '
        head+=cls['simple_name'] if n['constructor'] else safe_type(n['result'])+' '+n['name']
        params=[decl(p) for p in n['parameters']]
        if n.get('varargs'):params[-1]=params[-1].replace('[] '+n['parameters'][-1]['name'],'... '+n['parameters'][-1]['name'])
        head+='('+', '.join(params)+')'+(' throws '+', '.join(expr(x) for x in n['throws']) if n.get('throws') else '')
        return head+(' '+block(n['body'],depth) if n.get('body') else ';')
    def class_body(cls,depth):
        text='{\n';members=cls['members'];constants=[n for n in members if n.get('enum_constant')]
        if constants:
            text+='    '*(depth+1)+',\n'.join(n['name']+'('+', '.join(expr(x) for x in n['value']['arguments'])+')' for n in constants)+';\n'
        for n in members:
            if n.get('enum_constant'):continue
            if n['kind']=='VARIABLE':text+='    '*(depth+1)+variable(n)+';\n'
            elif n['kind']=='METHOD':text+=method(n,cls,depth+1)+'\n'
            elif n['kind']=='BLOCK':text+='    '*(depth+1)+('static ' if n['static'] else '')+block(n,depth+1)+'\n'
            else:text+=class_decl(n,depth+1)+'\n'
        return text+'    '*depth+'}'
    def class_decl(cls,depth):
        values=[x for x in cls['modifiers'] if not (cls['kind']=='ENUM' and x in ('final','abstract'))]
        head='    '*depth+mods(values)+' '+cls['kind'].lower()+' '+cls['simple_name']
        if cls['type_parameters']:head+='<'+', '.join(expr(x) for x in cls['type_parameters'])+'>'
        if cls.get('extends'):head+=' extends '+expr(cls['extends'])
        if cls['implements']:head+=(' extends ' if cls['kind']=='INTERFACE' else ' implements ')+', '.join(expr(x) for x in cls['implements'])
        return head+' '+class_body(cls,depth)
    files={}
    # One compilation unit may contain package-private companion classes.
    units={}
    for cls in module['classes']:units.setdefault(cls['origin']['source'],[]).append(cls)
    for origin,classes in units.items():
        first=classes[0];package=first['name'].rsplit('.',1)[0]
        imports='\n'.join('import '+('static ' if i['static'] else '')+i['qualified']+';' for i in first['imports'])
        for i in first['imports']:require(re.fullmatch(r'[\w.$*]+',i['qualified']),'Invalid import')
        text='package '+package+';\n'+imports+'\n// Generated from attributed editable regions, no source-body copying.\n'
        text+='\n'.join(class_decl(c,0) for c in classes)+'\n'
        files[package.replace('.','/')+'/'+origin.replace('\\','/').rsplit('/',1)[-1]]=text
    return files
