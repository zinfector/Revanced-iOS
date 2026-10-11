"""Compile fresh observation code on the established macOS/Xcode route."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(root, out):
    root, out = Path(root).resolve(), Path(out).resolve()
    request = json.loads((root / "diagnostic-inputs.json").read_text())
    for name, digest in request["sources"].items():
        if sha(root / name) != digest: raise ValueError("Diagnostic source drift")
    if not subprocess.check_output(["xcodebuild", "-version"], text=True).startswith("Xcode 16.4\n"):
        raise ValueError("Pinned Xcode 16.4 required")
    out.mkdir(parents=True, exist_ok=False)
    sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True).strip()
    dylib = out / "GeneratedObservations.dylib"
    command = ["xcrun", "--sdk", "iphoneos", "clang", "-arch", "arm64", "-isysroot", sdk,
               "-miphoneos-version-min=17.0", "-fobjc-arc", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-parameter", "-dynamiclib", "-framework", "Foundation",
               "-Wl,-headerpad,0x4000", "-Wl,-install_name,@executable_path/Frameworks/GeneratedObservations.dylib",
               str(root / "GeneratedObservations.m"), "-o", str(dylib)]
    with (out / "compile.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode: raise RuntimeError("Generated native observation compilation failed; see compile.log")
    symbols = subprocess.check_output(["xcrun", "nm", "-gU", str(dylib)], text=True)
    for name in ("_RVGInstallObservations", "_RVGObservationSnapshot"):
        if name not in symbols: raise ValueError("Missing generated diagnostic export: " + name)
    (out / "symbols.txt").write_text(symbols)
    receipt = {"schema": "installer.diagnostic-native-build.v1", "sources": request["sources"], "image": request["image"],
               "command": command, "payload_sha256": sha(dylib), "arm64_ios_compiled": True,
               "probes": request["probes"], "policies_enabled": False, "automatically_installed": False,
               "legacy_ios_feature_code_used": False, "device_verified": False}
    (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", type=Path, required=True); p.add_argument("--out", type=Path, required=True)
    a = p.parse_args(); build(a.source, a.out)
