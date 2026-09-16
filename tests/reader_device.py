"""Opt-in live reader acceptance test. Creates/reuses an original test book on SD.

Requires firmware 0.4.0. Changes the displayed book, font, place and bookmark.
Never deletes user data. Pass --exercise to authorize these test mutations.
"""
import argparse
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request

parser = argparse.ArgumentParser()
parser.add_argument("base")
parser.add_argument("--exercise", action="store_true", required=True)
args = parser.parse_args()
base = args.base.rstrip("/")

def request(path, data=None, token=None, content_type=None, expected=200):
    headers = {}
    if token:
        headers["X-Paper-Token"] = token
    if content_type:
        headers["Content-Type"] = content_type
    req = urllib.request.Request(base + path, data=data, headers=headers)
    try:
        response = urllib.request.urlopen(req, timeout=15)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.status == expected, (path, response.status, body)
        return json.loads(body) if "application/json" in response.headers.get("Content-Type", "") else body

token = re.search(rb'name="paper-token" content="([a-f0-9]+)"', request("/books")).group(1).decode()

def ready():
    deadline = time.monotonic() + 100
    while time.monotonic() < deadline:
        state = request("/api/reader")
        if not state["busy"]:
            return state
        time.sleep(1)
    raise AssertionError("Reader remained busy for 100 seconds")

def action(name, **extra):
    body = urllib.parse.urlencode(dict(action=name, **extra)).encode()
    request("/api/reader/action", body, token, "application/x-www-form-urlencoded", 202)
    state = ready()
    print(name, state, flush=True)
    return state

ready()
request("/api/reader/action", b"action=next", expected=403)
for body in [b"action=open&id=", b"action=open&id=../../x", b"action=font&value=5", b"action=jump&value=-1"]:
    request("/api/reader/action", body, token, "application/x-www-form-urlencoded", 400)
books = request("/api/books")["books"]
title = "Paper OS reader acceptance"
book = next((b for b in books if b["title"] == title), None)
sample = ("A quiet morning\n\n" + "The window looked out across a small garden. Light moved slowly over the leaves. There was time to read another page, and no need to hurry.\n\n" * 24).encode()
if not book:
    boundary = "paper-reader-test"
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="book"; filename="book.txt"\r\nContent-Type: text/plain\r\n\r\n').encode() + sample + f"\r\n--{boundary}--\r\n".encode()
    book = request("/api/books/upload?title=" + urllib.parse.quote(title), body, token, "multipart/form-data; boundary=" + boundary, 201)
assert request("/api/books/download?id=" + book["id"]) == sample
state = action("open", id=book["id"])
assert state["pages"] > 2 and state["view"] == "reading"
action("font", value=0)
state = action("jump", value=2)
offset = state["offset"]
assert state["page"] == 2 and offset > 0
assert action("bookmark")["hasBookmark"]
assert action("next")["page"] == 3
assert action("previous")["page"] == 2
state = action("font", value=2)
assert state["font"] == 2 and state["offset"] <= offset
action("font", value=0)
state = action("recall")
assert state["page"] == 2 and state["offset"] == offset
assert action("menu")["view"] == "menu"
assert action("resume")["page"] == 2
# Reload reads the per-book NVS record rather than the in-memory page index.
state = action("open", id=book["id"])
assert state["page"] == 2 and state["hasBookmark"]
assert action("sampler")["view"] == "sampler"
print("Reader device: upload/download, validation, pagination, bookmark, reflow and NVS reload passed. Font sampler left on screen.", flush=True)
