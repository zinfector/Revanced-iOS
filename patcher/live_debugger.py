"""Authenticated wired RVPort debugger. Native jobs survive transport reconnects."""
import argparse
import base64
import gzip
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid

import usb_diagnostics as usb
import live_debugger_lldb as external

ALIASES = {
    'status': 'debug.status', 'images': 'debug.images', 'regions': 'debug.regions',
    'read': 'debug.read', 'write': 'debug.write', 'rollback': 'debug.rollback',
    'allocate': 'debug.allocate', 'free': 'debug.free', 'object': 'debug.object',
    'get': 'debug.get', 'items': 'debug.items', 'value': 'debug.value',
    'invoke': 'debug.invoke', 'retain': 'debug.retain', 'release': 'debug.release',
    'functions': 'debug.functions', 'call': 'debug.call', 'bindings': 'debug.dearrow.bindings',
    'graph': 'debug.native.graph', 'roles': 'debug.native.roles',
    'refresh': 'debug.dearrow.refresh', 'events': 'debug.events',
    'job': 'debug.job', 'cancel': 'debug.cancel',
}
MUTATIONS = {'debug.write', 'debug.rollback', 'debug.allocate', 'debug.free',
             'debug.invoke', 'debug.call', 'debug.retain', 'debug.release',
             'debug.native.roles', 'debug.dearrow.refresh'}
TERMINAL = {'completed', 'failed', 'cancelled'}
PRIVATE = usb.ROOT/'.tools/live-debugger'


class DebugError(usb.DiagnosticError):
    pass


def output(value):
    print(json.dumps(value, indent=2, ensure_ascii=True), flush=True)


def private_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name+'.'+uuid.uuid4().hex+'.tmp')
    try:
        with os.fdopen(os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


class Connection(usb.DiagnosticConnection):
    def __init__(self, args):
        super().__init__(args)
        self.nonce = None

    def once(self, operation, arguments=None, request_id=None, nonce=None):
        device = self.wait_device()
        if self.token is None:
            self.token = usb.load_credential(device)
        identifier = request_id or str(uuid.uuid4())
        payload = {'schema': 2, 'operation': operation, 'credential': self.token,
                   'request_id': identifier, 'arguments': arguments or {}}
        if nonce:
            payload['process_nonce'] = nonce
        encoded = json.dumps(payload, separators=(',', ':'), allow_nan=False).encode()
        if len(encoded) > 8192:
            raise DebugError('Request exceeds the 8 KiB protocol limit.')
        with usb.connect(device, self.port) as sock:
            sock.sendall(struct.pack('!I', len(encoded))+encoded)
            length = struct.unpack('!I', usb.receive(sock, 4))[0]
            if not 0 < length <= usb.MAX_RESPONSE:
                raise DebugError('Invalid response length.')
            response = json.loads(usb.receive(sock, length))
        if not isinstance(response, dict):
            raise DebugError('Invalid response.')
        # The existing authentication gate deliberately uses schema 1 errors.
        if response.get('error') == 'authentication_required':
            raise usb.AuthenticationError('Saved PC pairing is missing or revoked. Run pair once.')
        if response.get('error') in {'bridge_paused', 'app_not_foreground'}:
            raise usb.RetryableError(response['error'])
        if response.get('schema') != 2 or response.get('request_id') != identifier:
            raise DebugError('Install RVPort 0.3.46 or later: live debug protocol is unavailable.')
        self.waiting = False
        return response

    def reliable(self, operation, arguments=None, request_id=None, nonce=None):
        # Only status/job/cancel use this. Job submission recovery is explicit below.
        while True:
            try:
                return self.once(operation, arguments, request_id, nonce)
            except (usb.RetryableError, OSError) as error:
                self.retry(error)

    def status(self):
        response = self.reliable('debug.status')
        if not response.get('ok'):
            raise DebugError(str(response.get('data', response)))
        data = response['data']
        self.nonce = data['process_nonce']
        return data

    def same_process(self, nonce):
        status = self.status()
        if status['process_nonce'] != nonce:
            raise DebugError('Process/debug mode changed. Outcome is unknown; pending job retained. Nothing was replayed.')
        if not status['enabled']:
            raise DebugError('Enable live debugging in Settings -> ReVanced -> USB diagnostics.')

    def journal(self, job):
        data = json.dumps(job, separators=(',', ':'), allow_nan=False).encode()
        if os.name == 'nt':
            data = usb.dpapi(data)
        private_write(PRIVATE/'pending'/f"{job['id']}.job", data)

    def clear(self, identifier, outcome=None):
        if outcome is not None:
            record = json.dumps(outcome, separators=(',', ':'), allow_nan=False).encode()
            if os.name == 'nt':
                record = usb.dpapi(record)
            private_write(PRIVATE/'completed'/f'{identifier}.job', record)
            retained = sorted((PRIVATE/'completed').glob('*.job'), key=lambda p: p.stat().st_mtime)
            for old in retained[:-64]:
                old.unlink()
        (PRIVATE/'pending'/f'{identifier}.job').unlink(missing_ok=True)

    def submit(self, operation, arguments):
        status = self.status()
        if not status['enabled']:
            raise DebugError('Enable live debugging in Settings -> ReVanced -> USB diagnostics.')
        if operation not in status.get('operations', []):
            raise DebugError('This app does not advertise the requested operation.')
        job = {'id': str(uuid.uuid4()), 'nonce': status['process_nonce'],
               'udid': self.udid, 'operation': operation, 'arguments': arguments,
               'created': time.time()}
        # Bound in-flight journals; never silently discard ambiguous mutations.
        if len(list((PRIVATE/'pending').glob('*.job'))) >= 64:
            raise DebugError('64 pending journals retained. Run recover before submitting more work.')
        self.journal(job)
        return self.finish(job, initial=True)

    def finish(self, job, initial=False):
        nonce, identifier = job['nonce'], job['id']
        if self.udid and self.udid.replace('-', '').lower() != job['udid'].replace('-', '').lower():
            raise DebugError('Pending job belongs to another saved device. Select its --udid.')
        self.udid = job['udid']
        self.same_process(nonce)
        response = None
        should_submit = initial
        while True:
            try:
                if should_submit:
                    response = self.once(job['operation'], job['arguments'], identifier, nonce)
                    should_submit = False
                else:
                    self.same_process(nonce)
                    response = self.once('debug.job', {'job_id': identifier}, nonce=nonce)
            except (usb.RetryableError, OSError) as error:
                self.retry(error)
                should_submit = False  # Recover by job identity, never blind replay.
                continue
            data = response.get('data', {})
            if response.get('process_nonce') != nonce:
                raise DebugError(f'Process changed; job {identifier} has unknown outcome. No replay.')
            if not response.get('ok'):
                reason = data.get('error', 'invalid_job_response')
                if reason == 'job_not_found':
                    # Same ID + nonce: server ledger/tombstone prevents repeating mutations.
                    should_submit = True
                    continue
                if reason == 'pending_job_limit':
                    should_submit = True
                    time.sleep(0.4)
                    continue
                if reason == 'job_result_expired_will_not_repeat':
                    raise DebugError(f'Result expired for job {identifier}; mutation was not repeated. Journal retained.')
                self.clear(identifier, response)
                raise DebugError(str(reason))
            if data.get('state') in TERMINAL:
                self.clear(identifier, data)
                return data
            if data.get('state') not in {'queued', 'running'}:
                raise DebugError('Invalid job state; journal retained.')
            time.sleep(0.2)

    def recover(self):
        records = []
        for path in sorted((PRIVATE/'pending').glob('*.job')):
            if path.stat().st_size > 32768:
                raise DebugError('Oversized pending journal; preserve it for manual inspection.')
            data = path.read_bytes()
            if os.name == 'nt':
                data = usb.dpapi(data, decrypt=True)
            job = json.loads(data)
            if self.udid and self.udid.replace('-', '').lower() != job['udid'].replace('-', '').lower():
                continue
            try:
                records.append({'journal': path.name, 'job': self.finish(job)})
            except DebugError as error:
                records.append({'journal': path.name, 'error': str(error), 'retained': True})
        return records

    def command(self, name, arguments):
        operation = ALIASES.get(name, name)
        if operation == 'debug.status':
            return self.status()
        if operation in {'debug.job', 'debug.cancel'}:
            status = self.status()
            return self.reliable(operation, arguments, nonce=status['process_nonce'])
        if operation not in ALIASES.values():
            raise DebugError('Unknown command. Type help for available commands.')
        args = dict(arguments)
        # Human-facing hex arguments become wire base64; wire format also accepted.
        if operation == 'debug.write':
            for human, wire in [('hex', 'bytes'), ('expected_hex', 'expected')]:
                if human in args:
                    args[wire] = base64.b64encode(bytes.fromhex(args.pop(human))).decode()
        return self.submit(operation, args)


HELP = '''Commands: status, images, regions, read, write, rollback, allocate, free,
object, get, items, value, invoke, retain, release, functions, call, bindings,
graph, roles, refresh, events, job, cancel, recover, history, result, dump,
watch, doctor, lldb, quit.
Arguments are a JSON object after the command. Addresses are hex strings.
  read {"address":"0x1234","length":64}
  dump {"image_uuid":"loaded image UUID","offset":"0x1000","length":1048576}
  write {"address":"0x1234","hex":"0100","expected_hex":"0000"}
  rollback {"transaction":"UUID returned by write"}
  graph {"handle":"binding handle from bindings","cursor":0}
  get {"handle":"object handle","selector":"parent"}
  invoke {"handle":"object handle","selector":"setAlpha:","value":0.5}
  call {"function":"mach_timebase_info"}
  watch {"interval":1,"out":"optional-private-events.jsonl"}
  lldb {"lldb":"path to lldb.exe","python":"path to python.exe",
        "target":"local YouTube executable","symbols":"RVPort.dSYM"}
Memory read/write jobs are bounded; writes require expected bytes, reject
executable regions and return a rollback transaction. Ctrl+C stops waiting;
recover retrieves pending outcomes. No mutation is replayed into a new process.
LLDB is optional: it requires Developer Mode, a mounted DDI and get-task-allow.
The in-app agent works independently of those external-debugger prerequisites.'''


def dump(connection, arguments):
    length = arguments.get('length')
    if type(length) is not int or not 1 <= length <= 16*1024*1024:
        raise DebugError('Dump length must be 1..16 MiB; each native read is at most 64 KiB.')
    image = arguments.get('image_uuid')
    field = 'offset' if image else 'address'
    base = arguments.get(field)
    if not isinstance(base, str) or not base.startswith('0x'):
        raise DebugError('Supply address, or image_uuid and offset, as hex strings.')
    start = int(base, 16)
    if not 0 <= start < 2**64-length:
        raise DebugError('Invalid dump range.')
    status = connection.status()
    nonce = status['process_nonce']
    path = Path(arguments['out']).expanduser() if arguments.get('out') else PRIVATE/'captures'/f'{uuid.uuid4()}.jsonl.gz'
    path.parent.mkdir(parents=True, exist_ok=True)
    captured = 0
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as raw:
        with gzip.GzipFile(fileobj=raw, mode='wb') as stream:
            def record(value):
                stream.write((json.dumps(value, separators=(',', ':'))+'\n').encode())
            record({'schema': 1, 'kind': 'memory_dump', 'status': status, 'requested_length': length,
                    'start': base, 'image_uuid': image, 'atomic': False})
            try:
                while captured < length:
                    connection.same_process(nonce)
                    request = {field: hex(start+captured), 'length': min(65536, length-captured)}
                    if image:
                        request['image_uuid'] = image
                    job = connection.submit('debug.read', request)
                    if job['process_nonce'] != nonce or not job['result'].get('ok'):
                        raise DebugError('Chunk failed or process changed: '+str(job))
                    record({'offset': captured, 'job': job})
                    captured += job['result']['length']
            finally:
                record({'captured_length': captured, 'complete': captured == length, 'atomic': False})
    output({'capture': str(path), 'length': captured, 'compressed': True, 'atomic': False})


def watch(connection, arguments):
    interval = float(arguments.get('interval', 1))
    if not 0.2 <= interval <= 60:
        raise DebugError('Watch interval must be 0.2..60 seconds.')
    cursor, nonce = '0x0', None
    stream = None
    try:
        if arguments.get('out'):
            path = Path(arguments['out']).expanduser()
            path.parent.mkdir(parents=True, exist_ok=True)
            stream = os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'w', encoding='utf-8')
        while True:
            status = connection.status()
            if status['process_nonce'] != nonce:
                nonce, cursor = status['process_nonce'], '0x0'
                output({'watch_process': nonce, 'pid': status['pid']})
            result = connection.submit('debug.events', {'after': cursor})
            data = result['result']
            if not data.get('ok'):
                raise DebugError(str(data))
            for event in data['events']:
                output(event)
                if stream:
                    stream.write(json.dumps({'process_nonce': nonce, **event})+'\n')
                    stream.flush()
            cursor = data['last_sequence']
            time.sleep(interval)
    finally:
        if stream:
            stream.close()


def dispatch(connection, name, arguments):
    if not isinstance(arguments, dict):
        raise DebugError('Command arguments must be a JSON object.')
    if name == 'help':
        print(HELP)
    elif name == 'pair':
        connection.token = usb.pair(connection.wait_device(), connection.port)
    elif name == 'recover':
        output(connection.recover())
    elif name == 'history':
        output([{'job_id': path.stem, 'completed': path.stat().st_mtime}
                for path in sorted((PRIVATE/'completed').glob('*.job'), key=lambda p: p.stat().st_mtime)])
    elif name == 'result':
        identifier = str(uuid.UUID(arguments['job_id']))
        data = (PRIVATE/'completed'/f'{identifier}.job').read_bytes()
        output(json.loads(usb.dpapi(data, decrypt=True) if os.name == 'nt' else data))
    elif name == 'watch':
        watch(connection, arguments)
    elif name == 'dump':
        dump(connection, arguments)
    elif name == 'doctor':
        output(external.doctor(connection.status(), arguments))
    elif name == 'lldb':
        status = connection.status()
        images = connection.command('images', {})
        if not images['result'].get('ok'):
            raise DebugError('Cannot identify loaded images for attachment.')
        status['images'] = images['result']['images']
        external.launch(status, connection.udid, arguments, PRIVATE, verify_process=connection.status)
    else:
        output(connection.command(name, arguments))


def main():
    parser = argparse.ArgumentParser(description=__doc__, epilog=HELP,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--udid')
    parser.add_argument('--port', type=int, default=49629)
    parser.add_argument('--args', default='{}', help='JSON arguments for a single command')
    parser.add_argument('command', nargs='?', default='console')
    args = parser.parse_args()
    if args.command == 'devices':
        output(usb.devices())
        return 0
    if args.command == 'saved':
        output([{k: v for k, v in item.items() if k != 'path'} for item in usb.saved_devices()])
        return 0
    if args.command == 'install-lldb-tools':
        external.install_tools(json.loads(args.args), PRIVATE)
        return 0
    connection = Connection(args)
    if args.command != 'console':
        dispatch(connection, args.command, json.loads(args.args))
        return 0
    print('RVPort live debugger. Uses your saved USB pairing. Type help; Ctrl+C stops waiting.')
    while True:
        try:
            line = input('rvdbg> ').strip()
            if not line:
                continue
            name, _, text = line.partition(' ')
            if name in {'quit', 'exit'}:
                return 0
            dispatch(connection, name, json.loads(text) if text.strip() else {})
        except EOFError:
            return 0
        except KeyboardInterrupt:
            print('\nWaiting stopped. Any pending native job can be retrieved with recover.')
        except (usb.DiagnosticError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
            print(str(error), file=sys.stderr)


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print('\nWaiting stopped. Run recover to retrieve pending jobs.', file=sys.stderr)
        raise SystemExit(130)
    except (usb.DiagnosticError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)
