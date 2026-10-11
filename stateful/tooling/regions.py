"""Checked structured object/effect dialect; no raw-source escape hatch."""
import copy
import hashlib
import json
import re

SCHEMA = 'editable-code.patch.regions.v2'
NAME = re.compile(r'[A-Za-z_$][A-Za-z0-9_$]*\Z')
TYPE = re.compile(r'[A-Za-z_$][A-Za-z0-9_$.]*(?:\[\])*\Z')
OPERATORS = {'PLUS': '+', 'MINUS': '-', 'MULTIPLY': '*', 'DIVIDE': '/', 'REMAINDER': '%',
             'EQUAL_TO': '==', 'NOT_EQUAL_TO': '!=', 'LESS_THAN': '<', 'LESS_THAN_EQUAL': '<=',
             'GREATER_THAN': '>', 'GREATER_THAN_EQUAL': '>=', 'CONDITIONAL_AND': '&&',
             'CONDITIONAL_OR': '||', 'AND': '&', 'OR': '|', 'XOR': '^'}
LITERALS = {'BOOLEAN_LITERAL', 'INT_LITERAL', 'LONG_LITERAL', 'FLOAT_LITERAL', 'DOUBLE_LITERAL', 'STRING_LITERAL', 'NULL_LITERAL'}
KINDS = {*OPERATORS, *LITERALS, 'BLOCK', 'VARIABLE', 'RETURN', 'IF', 'IDENTIFIER', 'MEMBER_SELECT',
         'METHOD_INVOCATION', 'PARENTHESIZED', 'EXPRESSION_STATEMENT', 'ASSIGNMENT',
         'CONDITIONAL_EXPRESSION', 'LAMBDA_EXPRESSION', 'TRY', 'CATCH', 'SYNCHRONIZED',
         'NEW_CLASS', 'THROW', 'TYPE_CAST', 'WHILE_LOOP', 'UNARY_MINUS', 'LOGICAL_COMPLEMENT'}
MODIFIERS = {'public', 'private', 'protected', 'static', 'final', 'volatile', 'synchronized', 'transient', 'strictfp'}


class AdmissionError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise AdmissionError(message)


def hash_graph(module):
    return hashlib.sha256(json.dumps(module, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def walk(value):
    if isinstance(value, dict):
        if 'kind' in value:
            yield value
        for key, child in value.items():
            if key not in ('origin', 'captures'):
                yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def annotate(module, contracts):
    result = copy.deepcopy(module)
    fields = {f['symbol']: f for c in result['classes'] for f in c['fields']}
    for c in result['classes']:
        for method in c['methods']:
            write_targets={id(n['target']) for n in walk(method['body']) if n['kind']=='ASSIGNMENT'}
            for n in walk(method['body']):
                effects = []
                if n['kind'] == 'METHOD_INVOCATION':
                    effects = contracts.get(n['target'], {}).get('effects', ['call.internal'])
                elif n['kind'] == 'ASSIGNMENT':
                    target = n['target']
                    if target.get('field'):
                        field = fields.get(target['symbol'], {})
                        effects = ['state.volatile.write' if 'volatile' in field.get('modifiers', []) else 'state.write']
                elif n['kind'] in ('IDENTIFIER', 'MEMBER_SELECT') and n.get('field') and id(n) not in write_targets:
                    field = fields.get(n['symbol'], {})
                    effects = ['state.volatile.read' if 'volatile' in field.get('modifiers', []) else 'state.read']
                elif n['kind'] == 'NEW_CLASS': effects = ['object.allocate', 'call.constructor']
                elif n['kind'] == 'SYNCHRONIZED': effects = ['monitor.acquire', 'monitor.release_on_all_exits']
                elif n['kind'] == 'THROW': effects = ['exception.throw']
                elif n['kind'] == 'TRY': effects = ['exception.region']
                elif n['kind'] == 'LAMBDA_EXPRESSION': effects = ['closure.capture']
                n['effects'] = effects
            method['effects'] = sorted({e for n in walk(method['body']) for e in n.get('effects', [])})
    result['effect_semantics'] = 'Java left-to-right evaluation and ordered structured regions; callback bodies execute on invocation, not allocation'
    return result


def verify(module, contracts):
    require(module.get('schema') == SCHEMA and module.get('types_attributed') is True, 'Attributed v2 module required')
    classes = module['classes']; require(classes, 'No classes')
    declared_methods = {m['symbol'] for c in classes for m in c['methods']}
    fields = {f['symbol']: f for c in classes for f in c['fields']}
    frontier = []
    seen_classes = set()
    for cls in classes:
        require(TYPE.fullmatch(cls['name']) and cls['name'] not in seen_classes, 'Invalid/duplicate class name')
        seen_classes.add(cls['name'])
        require(NAME.fullmatch(cls['simple_name']), 'Invalid class identifier')
        for member in [*cls['fields'], *cls['methods']]:
            require(member['name'] == '<init>' or NAME.fullmatch(member['name']), 'Invalid member identifier')
        for owner in [cls, *cls['fields'], *cls['methods']]:
            require(set(owner['modifiers']) <= MODIFIERS, 'Unsupported modifier')
        require(len(fields) == sum(len(c['fields']) for c in classes), 'Duplicate field identity')
        require(len(declared_methods) == sum(len(c['methods']) for c in classes), 'Duplicate method identity')
        for root in [*cls['fields'], *(m['body'] for m in cls['methods']), *(p for m in cls['methods'] for p in m['parameters'])]:
            for n in walk(root):
                require(n['kind'] in KINDS and not n.get('unsupported'), 'Unsupported operation at ' + str(n.get('origin')))
                require('raw' not in n and 'text' not in n, 'Raw executable source is forbidden')
                for key in ('type', 'declared_type', 'cast_type'):
                    if key in n: require(TYPE.fullmatch(n[key]), 'Unsupported type ' + n[key])
                if n['kind'] == 'METHOD_INVOCATION' and n['target'] not in declared_methods and n['target'] not in contracts:
                    frontier.append(dict(target=n['target'], origin=n['origin']))
                if n['kind'] == 'NEW_CLASS' and n['class'] not in seen_classes and not any(c['name'] == n['class'] for c in classes):
                    if n['target'] not in contracts: frontier.append(dict(target=n['target'], origin=n['origin']))
                if n['kind'] in ('IDENTIFIER', 'MEMBER_SELECT') and n.get('field') and n['symbol'] not in fields:
                    require('constant' in n or n['symbol'] in module.get('external_fields', {}), 'Unresolved field ' + n['symbol'])
                if n['kind'] == 'VARIABLE':
                    require(NAME.fullmatch(n['name']), 'Invalid variable identifier')
                    require(set(n['modifiers']) <= MODIFIERS, 'Unsupported variable modifier')
                if n['kind'] == 'LAMBDA_EXPRESSION':
                    require(not any(v.get('this') or (v.get('field') and 'static' not in v.get('modifiers',[]) and not v.get('receiver')) for v in walk(n['body'])),
                            'Implicit receiver capture requires an explicit object-lifetime adapter')
                    declared = {v['symbol'] for v in walk(n['body']) if v['kind'] == 'VARIABLE'} | {p['symbol'] for p in n['parameters']}
                    used = {v['symbol'] for v in walk(n['body']) if v['kind'] in ('IDENTIFIER', 'MEMBER_SELECT') and not v.get('field') and 'symbol' in v and 'constant' not in v}
                    expected = used - declared
                    require(expected == {v['symbol'] for v in n['captures']}, 'Closure capture inventory mismatch')
        for method in cls['methods']:
            require(TYPE.fullmatch(method['result']), 'Invalid method result')
    require(not frontier, 'Unresolved dependency frontier: ' + json.dumps(frontier))
    # Emission/re-attribution provides independent lexical, overload and type checks.
    return dict(classes=len(classes), methods=len(declared_methods), fields=len(fields), unresolved_frontier=frontier)


def java_type(value):
    require(TYPE.fullmatch(value), 'Unsupported Java type')
    return value


def literal(value, typ):
    if value is None: return 'null'
    if typ == 'boolean':
        require(type(value) is bool, 'Invalid Boolean literal'); return str(value).lower()
    if typ == 'java.lang.String':
        require(isinstance(value, str), 'Invalid string'); return json.dumps(value, ensure_ascii=True)
    if typ == 'long':
        require(re.fullmatch(r'-?[0-9]+', str(value)), 'Invalid long'); return str(value) + 'L'
    require(type(value) in (int, float), 'Invalid numeric literal')
    if typ in ('float', 'double'):
        import math
        require(math.isfinite(value), 'Nonfinite literal requires explicit Java operation')
        return repr(float(value)) + ('f' if typ == 'float' else 'd')
    return str(value)


def emit_java(module, contracts):
    verify(module, contracts)
    files = {}
    for cls in module['classes']:
        own_fields = {f['symbol'] for f in cls['fields']}
        def expr(n):
            k = n['kind']
            if k in LITERALS: return literal(n.get('value'), n['type'])
            if k in ('IDENTIFIER', 'MEMBER_SELECT'):
                if 'constant' in n and n.get('symbol') not in own_fields: return literal(n['constant'], n['type'])
                if n.get('field'):
                    if 'static' in n['modifiers']: return n['owner'] + '.' + n['name']
                    return (expr(n['receiver']) if n.get('receiver') else 'this') + '.' + n['name']
                if 'symbol' in n: return n['name']
                if 'class' in n: return n['class']
                if n.get('this'): return 'this'
                raise AdmissionError('Unknown reference')
            if k in OPERATORS: return '(' + expr(n['left']) + ' ' + OPERATORS[k] + ' ' + expr(n['right']) + ')'
            if k == 'PARENTHESIZED': return '(' + expr(n['value']) + ')'
            if k == 'UNARY_MINUS': return '(-' + expr(n['value']) + ')'
            if k == 'LOGICAL_COMPLEMENT': return '(!' + expr(n['value']) + ')'
            if k == 'ASSIGNMENT': return expr(n['target']) + ' = ' + expr(n['value'])
            if k == 'METHOD_INVOCATION':
                contract = contracts.get(n['target'], {})
                args = [expr(a) for a in n['arguments']]
                if contract.get('emit_call'):
                    return contract['emit_call'] + '(' + ', '.join(args) + ')'
                receiver = n['owner'] if n['static'] else (expr(n['receiver']) if n.get('receiver') else 'this')
                return receiver + '.' + n['name'] + '(' + ', '.join(args) + ')'
            if k == 'LAMBDA_EXPRESSION':
                params = ', '.join(p['name'] for p in n['parameters'])
                body = stmt(n['body'], 0) if n['body']['kind'] == 'BLOCK' else expr(n['body'])
                return '(' + params + ') -> ' + body
            if k == 'NEW_CLASS': return 'new ' + n['class'] + '(' + ', '.join(expr(a) for a in n['arguments']) + ')'
            if k == 'TYPE_CAST': return '((' + java_type(n['cast_type']) + ')' + expr(n['value']) + ')'
            if k == 'CONDITIONAL_EXPRESSION': return '(' + expr(n['condition']) + ' ? ' + expr(n['then']) + ' : ' + expr(n['else']) + ')'
            raise AdmissionError('Cannot emit expression ' + k)

        def decl(n):
            mods = ' '.join(n['modifiers'])
            return (mods + ' ' if mods else '') + java_type(n['declared_type']) + ' ' + n['name']

        def stmt(n, depth):
            pad = '    ' * depth; k = n['kind']
            if k == 'BLOCK': return '{\n' + ''.join(stmt(x, depth + 1) + '\n' for x in n['statements']) + pad + '}'
            if k == 'VARIABLE': return pad + decl(n) + (' = ' + expr(n['value']) if n.get('value') else '') + ';'
            if k == 'EXPRESSION_STATEMENT': return pad + expr(n['value']) + ';'
            if k == 'RETURN': return pad + 'return' + (' ' + expr(n['value']) if n.get('value') else '') + ';'
            if k == 'IF': return pad + 'if (' + expr(n['condition']) + ') ' + controlled(n['then'], depth) + (' else ' + controlled(n['else'], depth) if n.get('else') else '')
            if k == 'TRY':
                text = pad + 'try ' + stmt(n['body'], depth)
                for c in n['catches']: text += ' catch (' + decl(c['parameter']) + ') ' + stmt(c['body'], depth)
                if n.get('finally'): text += ' finally ' + stmt(n['finally'], depth)
                return text
            if k == 'SYNCHRONIZED': return pad + 'synchronized (' + expr(n['lock']) + ') ' + stmt(n['body'], depth)
            if k == 'THROW': return pad + 'throw ' + expr(n['value']) + ';'
            if k == 'WHILE_LOOP': return pad + 'while (' + expr(n['condition']) + ') ' + controlled(n['body'], depth)
            raise AdmissionError('Cannot emit statement ' + k)

        def controlled(n, depth):
            return stmt(n, depth) if n['kind'] == 'BLOCK' else '{\n' + stmt(n, depth + 1) + '\n' + '    ' * depth + '}'

        package = cls['name'].rsplit('.', 1)[0]
        text = 'package ' + package + ';\n\n// Generated from checked editable regions.\n'
        text += ' '.join(cls['modifiers']) + ' class ' + cls['simple_name'] + ' {\n'
        for field in cls['fields']:
            text += '    ' + decl(field) + (' = ' + expr(field['value']) if field.get('value') else '') + ';\n'
        for method in cls['methods']:
            head = ' '.join(method['modifiers']) + ' '
            head += cls['simple_name'] if method['constructor'] else java_type(method['result']) + ' ' + method['name']
            text += '    ' + head + '(' + ', '.join(decl(p) for p in method['parameters']) + ') ' + stmt(method['body'], 1) + '\n'
        text += '}\n'
        files[cls['name'].replace('.', '/') + '.java'] = text
    return files


def semantic_form(module, contracts):
    """Independent re-attribution comparison, allowing spelling-only changes."""
    reverse = {}
    for target, contract in contracts.items():
        if 'emit_call' in contract:
            owner, name = contract['emit_call'].rsplit('.', 1)
            # The initial rewritten operation has no parameters.
            reverse[owner + '#' + name + '()'] = target
    forms=[]
    for cls in module['classes']:
        locals_ = {n['symbol']: n['name'] for n in walk(cls) if n['kind']=='VARIABLE'}
        def normalize(v):
            if isinstance(v,list): return [normalize(x) for x in v]
            if not isinstance(v,dict): return v
            k=v.get('kind')
            if k=='PARENTHESIZED': return normalize(v['value'])
            if k in LITERALS or (k in ('IDENTIFIER','MEMBER_SELECT') and 'constant' in v):
                return dict(kind='constant',type=v['type'],value=v.get('constant',v.get('value')))
            if k in ('IDENTIFIER','MEMBER_SELECT') and 'symbol' in v:
                receiver = v.get('receiver')
                if receiver and (receiver.get('this') or receiver.get('name') == 'this'):
                    receiver = None
                return dict(kind='reference',type=v['type'],symbol=locals_.get(v['symbol'],v['symbol']),
                            receiver=normalize(receiver))
            d={key:normalize(child) for key,child in v.items() if key not in ('id','origin','effects','captures','effect_semantics','direct_effects')}
            if 'symbol' in d:d['symbol']=locals_.get(d['symbol'],d['symbol'])
            if k=='METHOD_INVOCATION':
                d['target']=reverse.get(d['target'],d['target'])
                d.pop('owner',None);d.pop('name',None)
            if k=='IF':
                for branch in ('then','else'):
                    if d.get(branch) and d[branch]['kind']!='BLOCK':d[branch]=dict(kind='BLOCK',statements=[d[branch]])
            if k=='WHILE_LOOP' and d['body']['kind']!='BLOCK':d['body']=dict(kind='BLOCK',statements=[d['body']])
            return d
        forms.append(normalize(cls))
    return forms
