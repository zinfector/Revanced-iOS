"""Normalize Objective-C method encodings without erasing array/bitfield sizes."""
import re


def abi(text):
    if not isinstance(text, str) or not text or len(text) > 4096:
        raise ValueError('Invalid ABI length')

    def token(i, depth=0):
        if depth > 64: raise ValueError('ABI nesting limit')
        start = i
        while i < len(text) and text[i] in 'rnNoORV': i += 1
        if i == len(text): raise ValueError('Incomplete ABI')
        c = text[i]; i += 1
        if c == '^': _, i = token(i, depth + 1)
        elif c in '{[(':
            close = {'{':'}', '[':']', '(' : ')'}[c]
            if c == '[':
                end = i
                while i < len(text) and text[i].isdigit(): i += 1
                if i == end: raise ValueError('Missing array count')
                _, i = token(i, depth + 1)
            else:
                while i < len(text) and text[i] not in '=' + close: i += 1
                if i < len(text) and text[i] == '=':
                    i += 1
                    while i < len(text) and text[i] != close:
                        if text[i] == '"':
                            i = text.index('"', i + 1) + 1
                        else: _, i = token(i, depth + 1)
            if i >= len(text) or text[i] != close: raise ValueError('Incomplete compound ABI')
            i += 1
        elif c == 'b':
            end = i
            while i < len(text) and text[i].isdigit(): i += 1
            if i == end: raise ValueError('Missing bitfield width')
        elif c == '@':
            if i < len(text) and text[i] == '?': i += 1
            elif i < len(text) and text[i] == '"': i = text.index('"', i + 1) + 1
        elif c not in 'cislqCISLQfdDBv*#:?': raise ValueError('Unknown ABI type: ' + c)
        return text[start:i], i

    parts, i = [], 0
    while i < len(text):
        part, i = token(i); parts.append(part)
        m = re.match(r'[+-]?\d+', text[i:])
        if m: i += len(m[0])
    return ''.join(parts)
