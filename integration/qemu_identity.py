"""Boot the built ESP32 firmware three times against one emulated flash image.

Run inside the ESP-IDF environment after building. Emulator evidence only; this
cannot demonstrate physical entropy quality or hardware power-loss behavior.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build_dir", type=pathlib.Path)
args = parser.parse_args()
build = args.build_dir.resolve()
config = json.loads((build / "flasher_args.json").read_text())
flash = build / "qemu_identity_flash.bin"
merge = [sys.executable, "-m", "esptool", "--chip", "esp32", "merge_bin",
         "-o", str(flash), "--fill-flash-size", "4MB"]
merge += config["write_flash_args"]
for offset, filename in config["flash_files"].items():
    merge += [offset, str(build / filename)]
subprocess.run(merge, check=True)
ids = []
for boot in range(1, 4):
    process = subprocess.Popen(
        ["qemu-system-xtensa", "-nographic", "-machine", "esp32",
         "-drive", f"file={flash},if=mtd,format=raw", "-no-reboot"],
        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        output, _ = process.communicate(timeout=8)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
    log = output.decode(errors="replace")
    (build / f"qemu-boot-{boot}.log").write_text(log)
    matches = re.findall(r"NeonDoll Body ID: ([0-9a-f]{32})", log)
    if len(matches) != 1 or "initialization failed" in log or "panic" in log.lower():
        sys.exit(f"Boot {boot} failed; inspect {build / f'qemu-boot-{boot}.log'}")
    ids.append(matches[0])
    print(f"Emulated boot {boot}: {ids[-1]}", flush=True)
if len(set(ids)) != 1:
    sys.exit("Body ID changed across emulated boots")
print("PASS: same Body ID over three emulator restarts; hardware proof pending")
