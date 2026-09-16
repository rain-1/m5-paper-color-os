"""Upload a built Paper OS app after the user unlocks OTA with the top button."""
import argparse
import json
from pathlib import Path
import re
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("base", help="Device URL, e.g. http://192.168.50.7")
parser.add_argument("firmware", type=Path)
args = parser.parse_args()
base = args.base.rstrip("/")

with urllib.request.urlopen(base + "/api/device", timeout=10) as response:
    state = json.load(response)
if state["otaSeconds"] < 15:
    raise SystemExit("Hold the top button C for 2.5 seconds to unlock OTA, then retry.")
if state.get("otaPowerOk") is False:
    raise SystemExit("Device rejected update power. Connect USB power or charge the battery.")
with urllib.request.urlopen(base + "/device", timeout=10) as response:
    token = re.search(rb'name="paper-token" content="([a-f0-9]+)"', response.read()).group(1).decode()

image = args.firmware.read_bytes()
if not 1024 <= len(image) <= 0x640000 or image[0] != 0xE9 or image[12:14] != b"\x09\x00":
    raise SystemExit("Expected an ESP32-S3 application firmware.bin fitting the OTA partition.")
boundary = "paper-os-firmware-upload"
body = ("--" + boundary + '\r\nContent-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        "Content-Type: application/octet-stream\r\n\r\n").encode() + image
body += ("\r\n--" + boundary + "--\r\n").encode()
request = urllib.request.Request(base + "/api/update?size=" + str(len(image)), data=body, headers={
    "X-Paper-Token": token,
    "Content-Type": "multipart/form-data; boundary=" + boundary,
})
with urllib.request.urlopen(request, timeout=120) as response:
    result = json.load(response)
assert result.get("ok"), result
print("Firmware accepted; device is restarting. Verify /api/device after reconnect.")
