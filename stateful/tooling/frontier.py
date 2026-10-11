"""Dependency admission report for any attributed module; never guesses providers."""
from regions import verify, AdmissionError, walk


def report(module, contracts):
    declared={m['symbol'] for c in module['classes'] for m in c['methods']}
    fields={f['symbol'] for c in module['classes'] for f in c['fields']}
    calls=[];unknown_fields=[];unsupported=[]
    for n in walk(module):
        if n['kind'] in ('METHOD_INVOCATION','NEW_CLASS') and n.get('target') not in declared and n.get('target') not in contracts:
            calls.append(dict(target=n.get('target'),origin=n.get('origin')))
        if n.get('field') and n['symbol'] not in fields and 'constant' not in n and n['symbol'] not in module.get('external_fields',{}):
            unknown_fields.append(dict(symbol=n['symbol'],type=n.get('type'),origin=n.get('origin')))
        if n.get('unsupported'):unsupported.append(dict(kind=n['kind'],origin=n.get('origin')))
    try: admission=verify(module,contracts);reason=None
    except AdmissionError as ex:admission=None;reason=str(ex)
    return dict(admitted=admission is not None,admission=admission,reason=reason,
                unresolved_calls=calls,unresolved_fields=unknown_fields,unsupported_operations=unsupported,
                provider_policy='Add reviewed reusable operation contracts; never substitute a native feature implementation')
