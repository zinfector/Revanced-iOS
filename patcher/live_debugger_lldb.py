"""Optional external LLDB attachment; never changes app signing or protections."""
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import uuid

PIN = '11.24.0'


def python_command(arguments, private):
    supplied = arguments.get('python')
    if supplied:
        return str(Path(supplied).resolve())
    candidate = private/'external-tools'/('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if candidate.is_file():
        return str(candidate)
    if not getattr(sys, 'frozen', False):
        return sys.executable
    return shutil.which('python')


def install_tools(arguments, private):
    executable = python_command(arguments, private)
    if not executable:
        raise ValueError('Install Python 3.11 or later, or supply {"python":"path to python.exe"}.')
    folder = private/'external-tools'
    target = folder/('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if not target.exists():
        subprocess.run([executable, '-m', 'venv', str(folder)], check=True)
    subprocess.run([str(target), '-m', 'pip', 'install', 'pymobiledevice3=='+PIN], check=True)
    print('External device tools installed. Supply a Darwin-capable LLDB via the lldb argument or PATH.')


def doctor(status, arguments):
    result = {'agent': status, 'lldb': arguments.get('lldb') or shutil.which('lldb'),
              'external_requirements': ['Darwin/remote-ios capable LLDB', 'Developer Mode enabled on iPhone',
                                       'Personalized developer disk image mounted', 'App signed with get-task-allow'],
              'agent_requires_debug_entitlement': False,
              'thread_control_requires_external_attachment': True,
              'device_attach_verified': False, 'pymobiledevice3_required_version': PIN}
    try:
        result['current_python_pymobiledevice3'] = importlib.metadata.version('pymobiledevice3')
    except importlib.metadata.PackageNotFoundError:
        result['current_python_pymobiledevice3'] = None
    result['attach_entitlement'] = 'present' if status.get('get_task_allow') is True else 'missing_or_unknown'
    result['next_step'] = ('install-lldb-tools, then lldb with local target/symbols' if status.get('get_task_allow') is True
                           else 'Agent commands work. External attachment needs a development profile allowing get-task-allow; this tool does not alter signing.')
    return result


def macho_uuid(path):
    with Path(path).open('rb') as stream:
        prefix = stream.read(8)
        offset = 0
        if prefix[:4] in (b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf'):
            count = struct.unpack('>I', prefix[4:8])[0]
            if not 0 < count <= 32:
                raise ValueError('Invalid universal Mach-O image.')
            wide = prefix[:4] == b'\xca\xfe\xba\xbf'
            selected = None
            for _ in range(count):
                record = stream.read(32 if wide else 20)
                cpu = struct.unpack_from('>I', record)[0]
                if cpu == 0x100000c:
                    selected = struct.unpack_from('>Q' if wide else '>I', record, 8)[0]
            if selected is None:
                raise ValueError('No ARM64 slice in target.')
            offset = selected
        stream.seek(offset)
        header = stream.read(32)
        if len(header) != 32 or header[:4] != b'\xcf\xfa\xed\xfe':
            raise ValueError('Expected a little-endian ARM64 Mach-O executable or DWARF image.')
        cpu, _, _, commands, length = struct.unpack_from('<5I', header, 4)
        if cpu != 0x100000c or commands > 4096 or length > 1024*1024:
            raise ValueError('Invalid ARM64 Mach-O command table.')
        data = stream.read(length)
        cursor = 0
        for _ in range(commands):
            if cursor+8 > len(data):
                break
            kind, size = struct.unpack_from('<II', data, cursor)
            if size < 8 or cursor+size > len(data):
                break
            if kind == 0x1b and size >= 24:
                return str(uuid.UUID(bytes=data[cursor+8:cursor+24])).upper()
            cursor += size
    raise ValueError('Target UUID was not found.')


def quote(value):
    # LLDB strings, never shell commands. Reject embedded command separators.
    value = str(value)
    if any(c in value for c in ('\n', '\r', '\0')):
        raise ValueError('Invalid path in LLDB command file.')
    return json.dumps(value.replace('\\', '/'))


def launch(status, udid, arguments, private, verify_process=None):
    if status.get('get_task_allow') is not True:
        raise ValueError('External attach unavailable: app get-task-allow is missing or unknown. Use agent commands, or sign using a compatible development profile.')
    debugger = arguments.get('lldb') or shutil.which('lldb')
    executable = python_command(arguments, private)
    if not debugger or not executable:
        raise ValueError('LLDB/Python missing. Run install-lldb-tools and supply a Darwin-capable lldb executable.')
    target = Path(arguments.get('target', '')).expanduser()
    if not target.is_file():
        raise ValueError('Supply target: the exact local YouTube executable from this installed IPA.')
    images = status.get('images', [])
    local_uuid = macho_uuid(target)
    matches = [image for image in images if image['uuid'].upper() == local_uuid]
    if len(matches) != 1:
        raise ValueError('Local target UUID does not match a loaded image. Attachment was not attempted.')
    pid = status.get('pid')
    if not isinstance(pid, int) or pid <= 0:
        raise ValueError('Invalid app PID.')
    port = int(arguments.get('local_port', 49630))
    if not 1024 <= port <= 65535 or port == 49629:
        raise ValueError('Invalid local debugger port.')
    # Refuse a pre-existing listener; do not connect LLDB to an unrelated server.
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', port))
    folder = private/'lldb'/str(uuid.uuid4())
    folder.mkdir(parents=True)
    commands = ['platform select remote-ios', 'target create '+quote(target)]
    library = arguments.get('library')
    if library:
        local_library = Path(library).expanduser()
        library_uuid = macho_uuid(local_library)
        if not any(image['name'] == 'RVPort.dylib' and image['uuid'].upper() == library_uuid for image in images):
            raise ValueError('Local RVPort library UUID does not match this installed app.')
        commands.append('target modules add '+quote(local_library))
    symbols = arguments.get('symbols')
    if symbols:
        source = Path(symbols).expanduser()
        dwarf = source/'Contents/Resources/DWARF/RVPort.dylib' if source.is_dir() else source
        symbol_uuid = macho_uuid(dwarf)
        if not any(image['name'] == 'RVPort.dylib' and image['uuid'].upper() == symbol_uuid for image in images):
            raise ValueError('RVPort symbols UUID does not match this installed app. No attach attempted.')
    # Matches pymobiledevice3's reviewed bundle-id attachment sequence.
    commands += [f'process connect connect://127.0.0.1:{port}', f'process attach --pid {pid}']
    if symbols:
        commands.append('target symbols add '+quote(source))
    command_file = folder/'attach.lldb'
    command_file.write_text('\n'.join(commands)+'\n', encoding='utf-8')
    checkpoint_file = folder/'checkpoints.json'
    checkpoints = {'process_nonce': status['process_nonce'], 'pid': pid,
                   'target_uuid': local_uuid, 'target_identity_verified': True,
                   'get_task_allow': True, 'symbols_identity_verified': bool(symbols),
                   'developer_image': 'negotiated_by_apple_service_not_yet_confirmed',
                   'debugserver_connection': 'pending', 'process_attach': 'inspect_lldb_output',
                   'memory_read': 'not_attempted', 'breakpoint_hit': 'not_attempted',
                   'expression_support': 'not_attempted'}
    checkpoint_file.write_text(json.dumps(checkpoints, indent=2), encoding='utf-8')
    log_path = folder/'transport.log'
    log = log_path.open('w', encoding='utf-8')
    environment = os.environ.copy()
    environment['PYMOBILEDEVICE3_UDID'] = udid
    environment['PYMOBILEDEVICE3_DEFAULT_FALLBACK'] = 'userspace'
    relay = subprocess.Popen([executable, '-u', '-m', 'pymobiledevice3', 'developer', 'debugserver',
                              'start-server', '--udid', udid, '--userspace', '--local-port', str(port),
                              '--host', '127.0.0.1'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors='replace', env=environment,
                             creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    ready = threading.Event()

    def drain():
        written = 0
        for line in relay.stdout:
            if written < 1024*1024:
                log.write(line)
                log.flush()
                written += len(line)
            if 'Started port forwarding' in line:
                ready.set()
        ready.set()

    reader = threading.Thread(target=drain, daemon=True)
    reader.start()
    try:
        print('Opening Apple debugproxy over the selected wired device. Ctrl+C cancels. Transport log: '+str(log_path))
        while not ready.wait(0.5):
            if relay.poll() is not None:
                break
        if relay.poll() is not None:
            checkpoints['debugserver_connection'] = 'relay_failed'
            checkpoint_file.write_text(json.dumps(checkpoints, indent=2), encoding='utf-8')
            raise ValueError('Apple debugproxy failed; inspect '+str(log_path)+' for Developer Mode, DDI or tunnel errors.')
        # The ready line precedes the asynchronous listener bind. Do not probe it
        # with a TCP connection: that would consume the debugserver session.
        time.sleep(0.5)
        if verify_process is not None:
            fresh = verify_process()
            if fresh.get('process_nonce') != status['process_nonce'] or fresh.get('pid') != pid:
                raise ValueError('App restarted during debugger setup. Attachment cancelled; run lldb again with fresh identity.')
        checkpoints['debugserver_connection'] = 'relay_started_attach_pending'
        checkpoint_file.write_text(json.dumps(checkpoints, indent=2), encoding='utf-8')
        print('LLDB opens interactively. After attach, use thread backtrace all, register read, breakpoint set, memory read/write, and expression. Detach before quitting to resume YouTube.')
        subprocess.run([debugger, '--source', str(command_file)], check=True)
    finally:
        if relay.poll() is None:
            relay.terminate()
            try:
                relay.wait(timeout=5)
            except subprocess.TimeoutExpired:
                relay.kill()
                relay.wait()
        reader.join(timeout=2)
        log.close()
        checkpoints['session_closed'] = True
        checkpoint_file.write_text(json.dumps(checkpoints, indent=2), encoding='utf-8')
