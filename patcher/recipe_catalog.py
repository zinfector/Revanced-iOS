"""Audit active native hook declarations and expand constant Objective-C loops.

This intentionally supports a small, explicit expression language. An unknown
declaration is an error, never an assumed compatible hook. Generated recipes are
bound to the payload and retain source locations and feature ownership.
"""
import hashlib
import itertools
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
HOOKS = {'RVHook', 'RVAdHook', 'RVDAUIHook', 'RVDAWatchHook', 'RVBoolGate', 'RVUIRequire'}
FORWARDERS = {'RVHook', 'RVHookAt', 'RVAdHook', 'RVDAUIHook', 'RVDAWatchHook', 'RVBoolGate', 'RVUIRequire'}
CAPABILITIES = {
    'background_playback':['YTIPlayabilityStatus|-|isPlayableInBackground|B@:'],
    'video_ads':['YTIPlayerResponse|-|playerAdsArray|@@:', 'YTIPlayerResponse|-|adPlacementsArray|@@:',
                 'YTIPlayerResponse|-|adSlotsArray|@@:'],
    'sponsorblock':['YTLocalPlaybackController|-|singleVideo:currentVideoTimeDidChange:|v@:@@',
                   'YTLocalPlaybackController|-|seekToTime:toleranceBefore:toleranceAfter:seekSource:|v@:@ddi'],
    'sponsorblock_markers':['YTLocalPlaybackController|-|singleVideo:currentVideoTimeDidChange:|v@:@@'],
    'custom_speed_menu':['YTPlayerViewController|-|setPlaybackRate:|v@:f'],
    'remember_speed':['YTPlayerViewController|-|setPlaybackRate:|v@:f']}


def mask_comments(source):
    pattern = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/', re.S)
    return pattern.sub(lambda m: m[0] if m[0].startswith('"') else ''.join('\n' if c == '\n' else ' ' for c in m[0]), source)


def closing(source, start):
    pairs = {'(': ')', '[': ']', '{': '}'}
    stack, quote, escape = [pairs[source[start]]], False, False
    for p in range(start + 1, len(source)):
        c = source[p]
        if quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == '"': quote = False
        elif c == '"': quote = True
        elif c in pairs: stack.append(pairs[c])
        elif c == stack[-1]:
            stack.pop()
            if not stack: return p
    raise ValueError('Unbalanced native expression')


def split_args(expression):
    parts, start, p = [], 0, 0
    while p < len(expression):
        c = expression[p]
        if c in '([{': p = closing(expression, p) + 1; continue
        if c == '"':
            p += 1
            while p < len(expression):
                if expression[p] == '\\': p += 2; continue
                if expression[p] == '"': break
                p += 1
        elif c == ',': parts.append(expression[start:p].strip()); start = p + 1
        p += 1
    parts.append(expression[start:].strip())
    return parts


def statement_end(source, start):
    p = start
    while p < len(source):
        if source[p] in '([{': p = closing(source, p) + 1; continue
        if source[p] == '"':
            p += 1
            while source[p] != '"': p += 2 if source[p] == '\\' else 1
        elif source[p] == ';': return p + 1
        p += 1
    raise ValueError('Unterminated statement')


def value(expression, source, before, env, seen=()):
    expression = expression.strip()
    if expression in ('YES', 'NO'): return expression == 'YES'
    if re.fullmatch(r'@"(?:\\.|[^"\\])*"', expression): return json.loads(expression[1:])
    if expression.startswith('@[') and closing(expression, 1) == len(expression) - 1:
        return [value(x, source, before, env, seen) for x in split_args(expression[2:-1]) if x]
    if expression.startswith('@{') and closing(expression, 1) == len(expression) - 1:
        result = {}
        for entry in split_args(expression[2:-1]):
            m = re.fullmatch(r'(@"[^"]+")\s*:\s*(@"[^"]+")', entry)
            if not m: raise ValueError('Unsupported dictionary expression: ' + entry)
            result[value(m[1], source, before, env)] = value(m[2], source, before, env)
        return result
    m = re.fullmatch(r'\[(\w+) stringByAppendingString:(@"[^"]+")\]', expression)
    if m: return value(m[1], source, before, env, seen) + value(m[2], source, before, env, seen)
    m = re.fullmatch(r'(\w+)\[([^\]]+)\]', expression)
    if m:
        key = int(m[2]) if m[2].isdigit() else value(m[2], source, before, env, seen)
        return value(m[1], source, before, env, seen)[key]
    if expression in env: return env[expression]
    if not re.fullmatch(r'\w+', expression) or expression in seen:
        raise ValueError('Unsupported native expression: ' + expression)
    matches = list(re.finditer(r'(?:\*\s*|,)\s*' + re.escape(expression) + r'\s*=\s*', source[:before]))
    if not matches: raise ValueError('Unbound native variable: ' + expression)
    match = matches[-1]
    p = match.end()
    end = statement_end(source, p)
    assignment = split_args(source[p:end - 1])[0]
    return value(assignment, source, match.start(), env, (*seen, expression))


def active_sources(root=ROOT / 'native'):
    result, pending = {}, ['RVPort.m']
    while pending:
        name = pending.pop()
        if name in result: continue
        path = (root / name).resolve()
        if path.parent != root.resolve(): raise ValueError('Include outside native root')
        source = mask_comments(path.read_text(encoding='utf-8-sig'))
        result[name] = source
        for include in re.findall(r'^\s*#(?:include|import) "([^"/]+\.(?:inc|h))"', source, re.M):
            if (root / include).exists(): pending.append(include)
    return result


def fallback_features(name):
    if name.startswith('RVAd'): return ['video_ads', 'feed_ads', 'shorts_ads']
    if name.startswith('RVDeArrow'): return ['dearrow_titles', 'dearrow_thumbnails']
    if name.startswith(('RVVote', 'RVDislike', 'RVElementDislike', 'RVNativeDislike')):
        return ['return_dislikes', 'paired_vote_buttons', 'first_launch_ui', 'ryd_voting']
    if name.startswith('RVSpeed'): return ['custom_speed_menu', 'remember_speed']
    if name.startswith('RVSponsor'): return ['sponsorblock', 'sponsorblock_manual', 'sponsorblock_markers', 'sponsorblock_contribute']
    if name.startswith('RVAuthentication'): return ['sideload_auth_identity', 'sideload_auth_keychain']
    if name.startswith('RVMiniplayer'): return ['miniplayer_disable_drag', 'miniplayer_disable_horizontal_drag', 'miniplayer_disable_double_tap', 'miniplayer_hide_subtext', 'miniplayer_square_corners', 'miniplayer_hide_overlay_buttons', 'classic_miniplayer']
    if name.startswith('RVShorts'): return ['hide_shorts', 'shorts_ads']
    if name.startswith('RVNavigation'): return ['hide_home_navigation', 'hide_subscriptions_navigation', 'hide_library_navigation', 'hide_shorts_navigation', 'hide_navigation_labels']
    if name.startswith('RVSettings'): return ['core']
    return ['core']


def extract(root=ROOT / 'native'):
    from features import FEATURES
    contracts, errors, sites = {}, [], 0
    sources = active_sources(root)
    for name, source in sorted(sources.items()):
        functions = []
        for m in re.finditer(r'\b(?:static\s+)?(?:void|BOOL|id|NSString\s*\*|NSDictionary\s*\*)\s+(RV\w+)\s*\([^;{}]*\)\s*\{', source):
            opening = source.index('{', m.start())
            functions.append((m[1], opening, closing(source, opening)))
        loops = []
        for m in re.finditer(r'for\s*\(\s*(?:NSString|NSArray)\s*\*\s*(\w+)\s+in\s+', source):
            opening = source.index('(', m.start())
            end = closing(source, opening)
            expression = source[m.end():end].strip()
            body = end + 1
            while source[body].isspace(): body += 1
            finish = closing(source, body) + 1 if source[body] == '{' else statement_end(source, body)
            loops.append((m[1], expression, m.start(), body, finish))
        call_pattern = r'\b(' + '|'.join(sorted(HOOKS)) + r')\s*\('
        for m in re.finditer(call_pattern, source):
            owner = next((f for f, start, end in functions if start < m.start() < end), None)
            if owner in FORWARDERS: continue
            opening = source.index('(', m.start())
            args = split_args(source[opening + 1:closing(source, opening)])
            # Typed function declaration, not an invocation.
            if not args or args[0].startswith('NSString *'): continue
            line = source.count('\n', 0, m.start()) + 1
            sites += 1
            contexts = [{}]
            try:
                for variable, expression, begin, start, end in loops:
                    if start <= m.start() < end:
                        next_contexts = []
                        for env in contexts:
                            iterable = value(expression, source, begin, env)
                            if not isinstance(iterable, (list, dict)): raise ValueError('Nonconstant loop')
                            next_contexts.extend({**env, variable: item} for item in iterable)
                        contexts = next_contexts
                for env in contexts:
                    cls, selector = [value(x, source, m.start(), env) for x in args[:2]]
                    if m[1] == 'RVBoolGate':
                        expected, kind = 'B@:', '-'
                        features = [value(args[2], source, m.start(), env)]
                    else:
                        expected = value(args[2], source, m.start(), env)
                        kind = '-' if m[1] == 'RVDAWatchHook' else '+' if value(args[3], source, m.start(), env) else '-'
                        callback_source_reuse = ','.join(args[4:])
                        # Preserve ownership through source predicates and constant view maps.
                        tokens = re.findall(r'(?:RVEnabled|RVSourceFeatureEnabled)\(@"([^"]+)"\)', callback_source_reuse)
                        tokens += re.findall(r'@"([^"]+)"\s*:\s*@"', callback_source_reuse)
                        # Reviewed helper: format hooks delegate their feature tests here.
                        for helper, start, end in functions:
                            if helper == 'RVLimitFormats' and re.search(r'\bRVLimitFormats\s*\(', callback_source_reuse):
                                tokens += re.findall(r'(?:RVEnabled|RVSourceFeatureEnabled)\(@"([^"]+)"\)', source[start:end])
                        features = sorted(set(tokens).intersection(FEATURES))
                        if not features: features = fallback_features(name)
                    key = '|'.join((cls, kind, selector, expected))
                    record = contracts.setdefault(key, dict(id=key, class_name=cls, selector=selector, kind=kind,
                                                            abi=expected, features=[], sources=[], hook=False))
                    record['features'] = sorted(set(record['features'] + features))
                    record['hook'] |= m[1] != 'RVUIRequire'
                    record['sources'].append(dict(file=name, line=line, wrapper=m[1]))
            except (ValueError, TypeError, KeyError, IndexError) as ex:
                errors.append(dict(file=name, line=line, expression=','.join(args[:4]), error=str(ex)))
    source_hashes={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                   for p in sorted(root.rglob('*')) if p.is_file() and p.name!='RVAdaptiveCatalog.h'}
    return dict(schema=1, adapter_catalog='rvport-adaptive-1', baseline_release='0.3.62',
                native_source_sha256=source_hashes, required_capabilities=CAPABILITIES,
                active_sources=sorted(sources), source_call_sites=sites, unresolved=errors,
                contracts=sorted(contracts.values(), key=lambda c: c['id']))


def canonical(data): return json.dumps(data, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()
def digest(data): return hashlib.sha256(canonical(data)).hexdigest()


def generate():
    catalog = extract()
    if catalog['unresolved']: raise ValueError('Unresolved recipe declarations:\n' + json.dumps(catalog['unresolved'], indent=2))
    path = ROOT / 'recipes.json'
    path.write_bytes(json.dumps(catalog, indent=2).encode() + b'\n')
    ids = '\n'.join('    @"' + c['id'] + '",' for c in catalog['contracts'])
    capabilities = ',\n'.join('    @"' + feature + '":@[' + ','.join('@"'+key+'"' for key in keys)+']'
                             for feature,keys in sorted(CAPABILITIES.items()))
    (ROOT / 'native/RVAdaptiveCatalog.h').write_text('// Generated by recipe_catalog.py.\n'
        '#define RV_ADAPTIVE_CATALOG_SHA256 @"' + digest(catalog) + '"\n'
        '#define RV_ADAPTIVE_PAYLOAD_TAG "RVPORT_ADAPTIVE_1:' + digest(catalog) + '"\n'
        'static NSString * const RVAdaptiveContractIDs[]={\n' + ids + '\n};\n'
        'static NSDictionary *RVAdaptiveCapabilities(void) { return @{\n' + capabilities + '\n}; }\n', encoding='ascii')
    return catalog


if __name__ == '__main__':
    result = extract()
    print(json.dumps({'contracts':len(result['contracts']), 'call_sites':result['source_call_sites'], 'unresolved':result['unresolved']}, indent=2))
    if result['unresolved']: raise SystemExit(2)
    generate()
