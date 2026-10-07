"""Collect RVPort diagnostics over USB; no UI automation or extra Python packages."""
import argparse
import base64
from collections import Counter
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import getpass
import gzip
import hashlib
import json
import os
from pathlib import Path
import plistlib
import socket
import struct
import sys
import time
import uuid

PORT = 49629
MAX_RESPONSE = 16 * 1024 * 1024
TOPICS = ('dearrow', 'watch', 'all', 'adblock', 'speed', 'ryd', 'sponsorblock', 'navigation')
ROOT = Path(sys.executable).resolve().parent.parent if getattr(sys, 'frozen', False) else Path(__file__).resolve().parent


class DiagnosticError(Exception):
    pass


def receive(sock, count):
    result = bytearray()
    while len(result) < count:
        chunk = sock.recv(min(count-len(result), 65536))
        if not chunk:
            raise DiagnosticError('Connection closed. Keep YouTube foreground and enable its USB diagnostics session.')
        result.extend(chunk)
    return bytes(result)


def mux_socket():
    try:
        if os.name == 'nt':
            return socket.create_connection(('127.0.0.1', 27015), timeout=3)
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(3)
        sock.connect('/var/run/usbmuxd')
        return sock
    except OSError as error:
        raise DiagnosticError('Apple USB service is unavailable. Start Apple Mobile Device Service and connect the iPhone.') from error


def mux_send(sock, version, kind, body=b''):
    sock.sendall(struct.pack('<IIII', 16+len(body), version, kind, 1)+body)


def mux_read(sock):
    length, version, kind, tag = struct.unpack('<IIII', receive(sock, 16))
    if not 16 <= length <= 1024*1024:
        raise DiagnosticError('Invalid USB service frame size.')
    return version, kind, tag, receive(sock, length-16)


def mux_plist(sock, operation, **values):
    request = {'MessageType': operation, 'ClientVersionString': 'RVPort-USB-Diagnostics-1',
               'ProgName': 'RVPort USB Diagnostics', 'kLibUSBMuxVersion': 3, **values}
    mux_send(sock, 1, 8, plistlib.dumps(request))
    version, kind, tag, body = mux_read(sock)
    if version != 1:
        return None
    if kind != 8 or tag != 1:
        raise DiagnosticError('Unexpected USB service response.')
    response = plistlib.loads(body)
    if not isinstance(response, dict):
        raise DiagnosticError('Invalid USB service response.')
    return response


def devices():
    # Apple's Windows service can speak binary v0 or plist v1. No pairing
    # records, application inventory or device files are requested here.
    with mux_socket() as sock:
        result = mux_plist(sock, 'ListDevices')
    if result is not None:
        if not isinstance(result.get('DeviceList'), list):
            raise DiagnosticError('USB device listing failed: '+str(result.get('Number', 'unknown')))
        return [{'id': entry['DeviceID'], 'serial': entry['Properties']['SerialNumber'], 'protocol': 1}
                for entry in result['DeviceList']
                if entry.get('Properties', {}).get('ConnectionType') == 'USB'
                and isinstance(entry.get('DeviceID'), int)
                and isinstance(entry.get('Properties', {}).get('SerialNumber'), str)]
    found = {}
    with mux_socket() as sock:
        mux_send(sock, 0, 3)
        version, kind, tag, body = mux_read(sock)
        if version != 0 or kind != 1 or tag != 1 or len(body) != 4 or struct.unpack('<I', body)[0]:
            raise DiagnosticError('Apple USB service refused device discovery.')
        end = time.monotonic()+1
        for _ in range(128):
            remaining = end-time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(remaining)
            try:
                version, kind, _, body = mux_read(sock)
            except socket.timeout:
                break
            if version != 0:
                raise DiagnosticError('USB service changed protocol unexpectedly.')
            if kind == 4 and len(body) >= 268:
                device_id = struct.unpack_from('<I', body)[0]
                serial = body[6:262].split(b'\0', 1)[0].decode('ascii')
                found[device_id] = {'id': device_id, 'serial': serial, 'protocol': 0}
            elif kind == 5 and len(body) == 4:
                found.pop(struct.unpack('<I', body)[0], None)
    return list(found.values())


def choose_device(udid=None):
    available = devices()
    if udid:
        available = [item for item in available if item['serial'].replace('-', '').lower() == udid.replace('-', '').lower()]
    if not available:
        raise DiagnosticError('No matching wired iPhone. Connect, unlock and trust this PC.')
    if len(available) != 1:
        raise DiagnosticError('Multiple wired devices. Select one using --udid from the devices command.')
    return available[0]


def connect(device, port):
    sock = mux_socket()
    try:
        encoded_port = socket.htons(port)
        if device['protocol'] == 1:
            response = mux_plist(sock, 'Connect', DeviceID=device['id'], PortNumber=encoded_port)
            code = response.get('Number', -1) if response else -1
            if not response or response.get('MessageType') != 'Result' or code:
                raise DiagnosticError('Open YouTube and enable Settings > ReVanced > USB diagnostics. USB connection error: '+str(code))
        else:
            mux_send(sock, 0, 2, struct.pack('<IHH', device['id'], encoded_port, 0))
            version, kind, tag, body = mux_read(sock)
            if version != 0 or kind != 1 or tag != 1 or len(body) != 4 or struct.unpack('<I', body)[0]:
                raise DiagnosticError('Open YouTube and enable Settings > ReVanced > USB diagnostics.')
        sock.settimeout(15)
        return sock
    except BaseException:
        sock.close()
        raise


def request(device, port, operation, credential, topic='dearrow'):
    encoded = json.dumps({'schema': 1, 'operation': operation, 'credential': credential, 'topic': topic},
                         separators=(',', ':')).encode()
    with connect(device, port) as sock:
        sock.sendall(struct.pack('!I', len(encoded))+encoded)
        length = struct.unpack('!I', receive(sock, 4))[0]
        if not 0 < length <= MAX_RESPONSE:
            raise DiagnosticError('The app returned an invalid report size.')
        response = json.loads(receive(sock, length))
    if not isinstance(response, dict) or response.get('schema') != 1:
        raise DiagnosticError('Unsupported app diagnostic protocol.')
    if response.get('ok') is not True:
        reason = str(response.get('error', 'unknown_error'))
        hint = ' Enable a new session and run pair again.' if reason in ('authentication_required', 'session_expired') else ''
        raise DiagnosticError(reason+hint)
    return response


class Blob(ctypes.Structure):
    _fields_ = [('size', wintypes.DWORD), ('data', ctypes.POINTER(ctypes.c_ubyte))]


def dpapi(value, decrypt=False):
    buffer = ctypes.create_string_buffer(value)
    source = Blob(len(value), ctypes.cast(buffer, ctypes.POINTER(ctypes.c_ubyte)))
    output = Blob()
    crypt = ctypes.WinDLL('crypt32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    if decrypt:
        function = crypt.CryptUnprotectData
        function.argtypes = [ctypes.POINTER(Blob), ctypes.POINTER(ctypes.c_wchar_p), ctypes.POINTER(Blob),
                             ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(Blob)]
        args = (ctypes.byref(source), None, None, None, None, 1, ctypes.byref(output))
    else:
        function = crypt.CryptProtectData
        function.argtypes = [ctypes.POINTER(Blob), ctypes.c_wchar_p, ctypes.POINTER(Blob),
                             ctypes.c_void_p, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(Blob)]
        args = (ctypes.byref(source), 'RVPort USB diagnostic session', None, None, None, 1, ctypes.byref(output))
    function.restype = wintypes.BOOL
    if not function(*args):
        raise DiagnosticError('Windows could not protect/read the local session credential.')
    try:
        return ctypes.string_at(output.data, output.size)
    finally:
        kernel.LocalFree(ctypes.cast(output.data, ctypes.c_void_p))


def credential_path(device):
    name = hashlib.sha256(device['serial'].encode()).hexdigest()[:16]+'.json'
    return ROOT/'.tools/usb-diagnostics'/name


def save_credential(device, token):
    path = credential_path(device)
    path.parent.mkdir(parents=True, exist_ok=True)
    secret = token.encode('ascii')
    mode = 'windows-dpapi' if os.name == 'nt' else 'owner-only'
    if os.name == 'nt':
        secret = dpapi(secret)
    # Session token is never printed, placed in a report or sent in argv.
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
        json.dump({'schema': 1, 'protection': mode, 'credential': base64.b64encode(secret).decode('ascii')}, stream)
    if os.name != 'nt':
        path.chmod(0o600)


def load_credential(device):
    path = credential_path(device)
    if not path.is_file():
        raise DiagnosticError('No PC session. Enable USB diagnostics on the phone, then run pair.')
    if path.stat().st_size > 16384:
        raise DiagnosticError('Invalid stored diagnostic credential.')
    value = json.loads(path.read_text(encoding='utf-8'))
    secret = base64.b64decode(value['credential'], validate=True)
    if value['protection'] == 'windows-dpapi' and os.name == 'nt':
        secret = dpapi(secret, decrypt=True)
    elif value['protection'] != 'owner-only' or os.name == 'nt':
        raise DiagnosticError('Stored session belongs to another platform; pair again.')
    token = secret.decode('ascii')
    if len(token) != 32 or any(character not in '0123456789abcdefABCDEF' for character in token):
        raise DiagnosticError('Invalid session token; pair again.')
    return token


def pair(device, port, code=None):
    code = (code or getpass.getpass('Session code shown in YouTube: ')).strip()
    if len(code) != 8 or any(character not in '0123456789abcdefABCDEF' for character in code):
        raise DiagnosticError('The session code must contain eight hexadecimal characters.')
    response = request(device, port, 'pair', code)
    token = response.get('token')
    if not isinstance(token, str) or len(token) != 32:
        raise DiagnosticError('The app did not provide a valid session credential.')
    save_credential(device, token)
    print('Connected. Keep YouTube foreground; session expires after 30 minutes or backgrounding.')
    return token


def summarize(response):
    root = response['data']
    dearrow = root.get('dearrow', root if 'backend' in root and 'bindings' in root else {})
    watch = dearrow.get('watch_interactions', root if response.get('topic') == 'watch' else {})
    failures = Counter(str(item.get('blocker', 'unspecified')) for item in dearrow.get('attempt_samples', []))
    bindings = []
    keys = ('surface', 'route', 'video_hash', 'adapter', 'ownership_blocker', 'requested_parts', 'title_input_blocker',
            'image_input_blocker', 'title_consumer_verified', 'image_consumer_verified', 'title_blocker', 'image_blocker',
            'result_present', 'image_origin', 'encoded_image_ready', 'fallback_phase', 'duration_source',
            'refresh_requests', 'native_materializations', 'last_checkpoint', 'last_blocker', 'consumer_checkpoint')
    for item in dearrow.get('bindings', [])[:32]:
        bindings.append({key: item[key] for key in keys if key in item})
    return {'schema': 1, 'topic': response.get('topic'), 'patcher_version': root.get('patcher_version'),
            'build_source_sha256': root.get('build_source_sha256'), 'bridge': response.get('bridge', {}),
            'dearrow': {'capture_armed': dearrow.get('capture_armed'), 'counts': dearrow.get('counts', {}),
                       'backend_counts': dearrow.get('backend', {}).get('counts', {}),
                       'attempt_blockers': dict(failures), 'bindings': bindings, 'service_probe': dearrow.get('service_probe', {})},
            'watch': {'counts': watch.get('counts', {}), 'last_stages': watch.get('last_stages', {}),
                      'hooks': watch.get('hooks', []), 'active_watch': watch.get('active_watch')},
            'note': 'Full evidence retained separately. A bridge response or successful probe does not prove visible replacement or successful taps.'}


def save_report(response, folder, uncompressed=False):
    folder = Path(folder).expanduser().resolve()
    folder.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc)
    stem = 'USB-'+str(response.get('topic', 'report'))+'-'+now.strftime('%Y%m%d-%H%M%S-%f')+'-'+uuid.uuid4().hex[:6]
    envelope = {'host_capture_utc': now.isoformat(), **response}
    raw = json.dumps(envelope, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    path = folder/(stem+('.json' if uncompressed else '.json.gz'))
    with path.open('xb') as stream:
        stream.write(raw if uncompressed else gzip.compress(raw, compresslevel=6, mtime=0))
    summary = folder/(stem+'.summary.json')
    with summary.open('x', encoding='utf-8') as stream:
        json.dump({'host_capture_utc': now.isoformat(), 'full_report': path.name, **summarize(response)}, stream, indent=2)
        stream.write('\n')
    print('Saved '+str(path))
    print('Summary '+str(summary))
    return path


def interactive(args):
    print('Enable Settings > ReVanced > USB diagnostics on the wired iPhone.')
    device = choose_device(args.udid)
    try:
        token = load_credential(device)
        request(device, args.port, 'status', token)
        print('Using the current USB session.')
    except DiagnosticError:
        token = pair(device, args.port)
    menu = '\n1 Start DeArrow capture\n2 Fetch DeArrow report\n3 Fetch full report\n4 Fetch watch tap trace\n5 Show bridge status\n6 Stop capture\n7 Probe current video service\n0 Exit\n'
    while True:
        print(menu)
        choice = input('Choose: ').strip()
        if choice == '0':
            return
        try:
            if choice in ('2', '3', '4'):
                topic = {'2': 'dearrow', '3': 'all', '4': 'watch'}[choice]
                save_report(request(device, args.port, 'report', token, topic), args.reports, args.uncompressed)
            elif choice in ('1', '5', '6', '7'):
                operation = {'1': 'capture-start', '5': 'status', '6': 'capture-stop', '7': 'probe'}[choice]
                print(json.dumps(request(device, args.port, operation, token)['data'], indent=2))
            else:
                print('Choose one of the listed diagnostics commands.')
        except DiagnosticError as error:
            print(str(error))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--udid', help='Select a wired device when several are connected.')
    parser.add_argument('--port', type=int, default=PORT)
    parser.add_argument('--reports', type=Path, default=ROOT.parent/'Reports')
    parser.add_argument('--uncompressed', action='store_true', help='Save full JSON instead of gzip JSON.')
    commands = parser.add_subparsers(dest='command')
    commands.add_parser('devices', help='List wired devices through Apple USB service.')
    pairing = commands.add_parser('pair', help='Enter the short session code displayed in YouTube.')
    pairing.add_argument('--code-file', type=Path, help='Read the eight-character code from a local file instead of prompting.')
    for command in ('status', 'start', 'stop', 'probe'):
        commands.add_parser(command)
    fetch = commands.add_parser('fetch', help='Save a full report plus a concise checkpoint summary.')
    fetch.add_argument('--topic', choices=TOPICS, default='dearrow')
    record = commands.add_parser('record', help='Poll and save diagnostics while you reproduce the issue.')
    record.add_argument('--topic', choices=TOPICS, default='dearrow')
    record.add_argument('--duration', type=float, default=60)
    record.add_argument('--interval', type=float, default=5)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error('Port must be between 1 and 65535.')
    try:
        if not args.command:
            interactive(args)
            return 0
        if args.command == 'devices':
            print(json.dumps([{'udid': item['serial'], 'connection': 'USB', 'protocol': item['protocol']} for item in devices()], indent=2))
            return 0
        device = choose_device(args.udid)
        if args.command == 'pair':
            if args.code_file and args.code_file.stat().st_size > 256:
                raise DiagnosticError('Session-code file is too large.')
            code = args.code_file.read_text(encoding='utf-8-sig') if args.code_file else None
            pair(device, args.port, code)
            return 0
        token = load_credential(device)
        if args.command == 'fetch':
            save_report(request(device, args.port, 'report', token, args.topic), args.reports, args.uncompressed)
        elif args.command == 'record':
            if not 1 <= args.duration <= 1800 or not 1 <= args.interval <= 60:
                raise DiagnosticError('Duration must be 1..1800 seconds and interval 1..60 seconds.')
            deadline = time.monotonic()+args.duration
            while True:
                save_report(request(device, args.port, 'report', token, args.topic), args.reports, args.uncompressed)
                remaining = deadline-time.monotonic()
                if remaining <= 0:
                    break
                time.sleep(min(args.interval, remaining))
        else:
            operation = {'status': 'status', 'start': 'capture-start', 'stop': 'capture-stop', 'probe': 'probe'}[args.command]
            print(json.dumps(request(device, args.port, operation, token)['data'], indent=2))
        return 0
    except KeyboardInterrupt:
        print('\nCollector stopped. Existing reports retained.')
        return 130
    except (DiagnosticError, OSError, ValueError, KeyError, plistlib.InvalidFileException) as error:
        print('USB diagnostics: '+str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
