"""Live-device checks. --write-picture stores and displays one six-colour test card."""
import argparse
import datetime
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("base")
parser.add_argument("--write-picture", action="store_true")
args = parser.parse_args()


def request(path, data=None, headers=None):
    req = urllib.request.Request(args.base + path, data=data, headers=headers or {})
    try:
        with urllib.request.urlopen(req, timeout=20) as response:
            return response.status, response.read()
    except urllib.error.HTTPError as error:
        return error.code, error.read()


code, page = request("/gallery")
assert code == 200
token = re.search(rb'name="paper-token" content="([a-f0-9]+)"', page).group(1).decode()
headers = {"X-Paper-Token": token}
for path in ("/device", "/device.js", "/converter.js", "/api/device", "/api/card"):
    assert request(path)[0] == 200, path
assert request("/api/display", b"path=x")[0] == 403
assert request("/api/display", b"path=../../secret", headers)[0] == 400
assert request("/api/images?month=../../etc")[0] == 400
assert request("/connect", b"ssid=test&password=not-a-real-password")[0] == 403
assert request("/api/update?size=1024", b"invalid")[0] == 400


def upload(payload, authenticated=True):
    boundary = "paper-test-boundary"
    body = ("--" + boundary + '\r\nContent-Disposition: form-data; name="image"; filename="test.p6"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n").encode() + payload
    body += ("\r\n--" + boundary + "--\r\n").encode()
    h = dict(headers) if authenticated else {}
    h["Content-Type"] = "multipart/form-data; boundary=" + boundary
    return request("/api/upload?date=" + stamp, body, h)


stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
assert upload(b"bad")[0] == 400
assert upload(b"bad", False)[0] == 403
assert upload(b"x" * 120017)[0] == 400
print("Live HTTP routes, tokens, path validation, bad/oversize upload rejection: passed")

if args.write_picture:
    # One hundred rows each of white, yellow, red, blue, green, black.
    pixels = b"".join(bytes([(i << 4) | i]) * 20000 for i in (1, 2, 3, 4, 5, 0))
    payload = b"P6I1\x90\x01\x58\x02" + bytes(8) + pixels
    for attempt in range(8):
        code, body = upload(payload)
        if code != 409:
            break
        time.sleep(5)
    assert code == 201, (code, body)
    path = json.loads(body)["path"]
    month = stamp[:4] + "-" + stamp[4:6]
    code, body = request("/api/images?month=" + month)
    assert code == 200 and path in json.loads(body)["files"], body
    code, body = request("/api/display", urllib.parse.urlencode({"path": path}).encode(), headers)
    assert code == 202, body
    print("SD write/readback, gallery listing and display queue: passed")
    print("Test picture retained at", path)
