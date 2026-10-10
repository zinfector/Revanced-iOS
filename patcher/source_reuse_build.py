"""Native build preflight; also works with packaged generated-source snapshots."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parent


def ensure_native():
    pipeline=ROOT.parent/'source-reuse/pipeline.py'
    if pipeline.exists():
        subprocess.run([sys.executable,str(pipeline),'verify'],check=True,cwd=pipeline.parent)
    receipt=ROOT/'native/RVSourceOrigins.json'
    manifest=json.loads(receipt.read_text(encoding='utf-8'))
    for name,expected in manifest['native_sha256'].items():
        path=ROOT/'native'/name
        if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
            raise RuntimeError('Generated source origin mismatch: '+str(path))
