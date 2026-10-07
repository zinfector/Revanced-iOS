"""Bundle the GUI, supported profile, and built payload into one Windows executable."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent

def main():
    if sys.platform != 'win32': raise SystemExit('This packaging entry point targets Windows.')
    tools = ROOT/'.tools/packager'
    if not (tools/'PyInstaller').is_dir():
        raise SystemExit('Install first: python -m pip install --target .tools/packager pyinstaller==6.22.3')
    from patcher import payload
    payload(ROOT/'build/RVPort.dylib')
    env = os.environ.copy()
    env['PYTHONPATH'] = str(tools) + os.pathsep + env.get('PYTHONPATH', '')
    command = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--windowed',
               '--name', 'YouTube-iOS-Patcher', '--distpath', str(ROOT/'dist'),
               '--workpath', str(ROOT/'build/pyinstaller'), '--specpath', str(ROOT/'build'),
               '--paths', str(ROOT), '--add-data', str(ROOT/'profiles')+';profiles',
               '--add-data', str(ROOT/'build/RVPort.dylib')+';build', str(ROOT/'gui.py')]
    with (ROOT/'build/package.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((ROOT/'build/package.log').read_text(), file=sys.stderr)
        return result.returncode
    print(ROOT/'dist/YouTube-iOS-Patcher.exe')
    collector = [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--console',
                 '--name', 'YouTube-USB-Diagnostics', '--distpath', str(ROOT/'dist'),
                 '--workpath', str(ROOT/'build/pyinstaller-usb'), '--specpath', str(ROOT/'build'),
                 str(ROOT/'usb_diagnostics.py')]
    with (ROOT/'build/package-usb.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(collector, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((ROOT/'build/package-usb.log').read_text(), file=sys.stderr)
        return result.returncode
    print(ROOT/'dist/YouTube-USB-Diagnostics.exe')
    return 0

if __name__ == '__main__': raise SystemExit(main())
